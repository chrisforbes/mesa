/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file brw_vec4_ds.c
 *
 * State atom for client-programmable tessellation evaluation shaders, and support code.
 */

#include "brw_ds.h"
#include "brw_context.h"
#include "brw_vec4_ds_visitor.h"
#include "brw_state.h"


static bool
do_ds_prog(struct brw_context *brw,
           struct gl_shader_program *prog,
           struct brw_tess_eval_program *dp,
           struct brw_ds_prog_key *key)
{
   struct brw_stage_state *stage_state = &brw->ds.base;
   struct brw_ds_compile c;
   memset(&c, 0, sizeof(c));
   c.key = *key;
   c.dp = dp;

   switch(dp->program.Spacing) {
   case GL_EQUAL:
      c.prog_data.partitioning = integer;
      break;
   case GL_FRACTIONAL_ODD:
      c.prog_data.partitioning = odd_fractional;
      break;
   case GL_FRACTIONAL_EVEN:
      c.prog_data.partitioning = even_fractional;
      break;
   default:
      assert(0);
      c.prog_data.partitioning = integer;
   }

   switch(dp->program.PrimitiveMode) {
   case GL_QUADS:
      c.prog_data.domain = quad;
      break;
   case GL_TRIANGLES:
      c.prog_data.domain = tri;
      break;
   case GL_ISOLINES:
      c.prog_data.domain = isoline;
      break;
   default:
      assert(0);
      c.prog_data.domain = tri;
   }

   if (dp->program.PointMode) {
      c.prog_data.output_topology = point;
   } else if (dp->program.PrimitiveMode == GL_ISOLINES) {
      c.prog_data.output_topology = line;
   } else {
      switch(dp->program.VertexOrder) {
      case GL_CCW:
         c.prog_data.output_topology = tri_ccw;
         break;
      case GL_CW:
         c.prog_data.output_topology = tri_cw;
         break;
      default:
         assert(0);
         c.prog_data.output_topology = tri_ccw;
      }
   }

   c.prog_data.include_primitive_id =
      (dp->program.Base.InputsRead & VARYING_BIT_PRIMITIVE_ID) != 0;

   /* Allocate the references to the uniforms that will end up in the
    * prog_data associated with the compiled program, and which will be freed
    * by the state cache.
    *
    * Note: param_count needs to be num_uniform_components * 4, since we add
    * padding around uniform values below vec4 size, so the worst case is that
    * every uniform is a float which gets padded to the size of a vec4.
    */
   struct gl_shader *ds = prog->_LinkedShaders[MESA_SHADER_TESS_EVAL];
   int param_count = ds->num_uniform_components * 4;

   c.prog_data.base.base.param =
      rzalloc_array(NULL, const gl_constant_value *, param_count);
   c.prog_data.base.base.pull_param =
      rzalloc_array(NULL, const gl_constant_value *, param_count);
   /* Setting nr_params here NOT to the size of the param and pull_param
    * arrays, but to the number of uniform components vec4_visitor
    * needs. vec4_visitor::setup_uniforms() will set it back to a proper value.
    */
   c.prog_data.base.base.nr_params = ALIGN(param_count, 4) / 4 + ds->num_samplers;

   GLbitfield64 outputs_written = dp->program.Base.OutputsWritten;

   brw_compute_vue_map(brw, &c.prog_data.base.vue_map, outputs_written);

   unsigned output_size_bytes = c.prog_data.base.vue_map.num_slots;

   assert(output_size_bytes >= 1);
   if (output_size_bytes > GEN7_MAX_DS_URB_ENTRY_SIZE_BYTES)
      return false;

   /* URB entry sizes are stored as a multiple of 64 bytes. */
   c.prog_data.base.urb_entry_size = ALIGN(output_size_bytes, 64) / 64;

   brw_compute_tess_vue_map(brw, &c.input_vue_map, c.key.input_varyings,
                            dp->program.IsPatch);

   /* We always have our DS pull from the patch URB entry via messages. */
   c.prog_data.base.urb_read_length = 0;

   void *mem_ctx = ralloc_context(NULL);
   unsigned program_size;
   const unsigned *program =
      brw_ds_emit(brw, prog, &c, mem_ctx, &program_size);
   if (program == NULL) {
      ralloc_free(mem_ctx);
      return false;
   }

   /* Scratch space is used for register spilling */
   if (c.base.last_scratch) {
      perf_debug("Tessellation Evaluation shader triggered register "
                 "spilling.  Try reducing the number of live vec4 values to "
                 "improve performance.\n");

      c.prog_data.base.base.total_scratch
         = brw_get_scratch_size(c.base.last_scratch*REG_SIZE);

      brw_get_scratch_bo(brw, &stage_state->scratch_bo,
			 c.prog_data.base.base.total_scratch * brw->max_ds_threads);
   }

   brw_upload_cache(&brw->cache, BRW_CACHE_DS_PROG,
                    &c.key, sizeof(c.key),
                    program, program_size,
                    &c.prog_data, sizeof(c.prog_data),
                    &stage_state->prog_offset, &brw->ds.prog_data);
   ralloc_free(mem_ctx);

   return true;
}


static void
brw_upload_ds_prog(struct brw_context *brw)
{
   struct gl_context *ctx = &brw->ctx;
   struct brw_stage_state *stage_state = &brw->ds.base;
   struct brw_ds_prog_key key;
   /* BRW_NEW_TESS_EVAL_PROGRAM */
   struct brw_tess_eval_program *dp =
      (struct brw_tess_eval_program *) brw->tess_eval_program;

   if (dp == NULL) {
      /* No tessellation evaluation shader.
       * Vertex data just passes straight through.
       */
      if (brw->state.dirty.brw & BRW_NEW_VUE_MAP_VS) {
         brw->vue_map_ds_out = brw->vue_map_vs;
         brw->state.dirty.brw |= BRW_NEW_VUE_MAP_DS_OUT;
      }

      /* Other state atoms had better not try to access prog_data, since
       * there's no DS program.
       */
      brw->ds.prog_data = NULL;
      brw->ds.base.prog_data = NULL;

      return;
   }

   struct gl_program *prog = &dp->program.Base;

   memset(&key, 0, sizeof(key));

   key.base.program_string_id = dp->id;
   brw_setup_vue_key_clip_info(brw, &key.base,
                                dp->program.Base.UsesClipDistanceOut);

   /* _NEW_TEXTURE */
   brw_populate_sampler_prog_key_data(ctx, prog, stage_state->sampler_count,
                                      &key.base.tex);

   /* BRW_NEW_VUE_MAP_VS */
   key.input_varyings = brw->vue_map_hs_out.slots_valid;

   if (!brw_search_cache(&brw->cache, BRW_CACHE_DS_PROG,
                         &key, sizeof(key),
                         &stage_state->prog_offset, &brw->ds.prog_data)) {
      bool success =
         do_ds_prog(brw, ctx->Shader.CurrentProgram[MESA_SHADER_TESS_EVAL], dp,
                    &key);
      assert(success);
      (void)success;
   }
   brw->ds.base.prog_data = &brw->ds.prog_data->base.base;

   if (memcmp(&brw->vs.prog_data->base.vue_map, &brw->vue_map_ds_out,
              sizeof(brw->vue_map_ds_out)) != 0) {
      brw->vue_map_ds_out = brw->ds.prog_data->base.vue_map;
      brw->state.dirty.brw |= BRW_NEW_VUE_MAP_DS_OUT;
   }
}


const struct brw_tracked_state brw_ds_prog = {
   .dirty = {
      .mesa  = _NEW_TEXTURE,
      .brw   = BRW_NEW_TESS_EVAL_PROGRAM |
               BRW_NEW_VUE_MAP_VS |
               BRW_NEW_VUE_MAP_HS_OUT,
   },
   .emit = brw_upload_ds_prog
};


bool
brw_ds_precompile(struct gl_context *ctx,
                  struct gl_shader_program *shader_prog,
                  struct gl_program *prog)
{
   struct brw_context *brw = brw_context(ctx);
   struct brw_ds_prog_key key;
   uint32_t old_prog_offset = brw->ds.base.prog_offset;
   struct brw_ds_prog_data *old_prog_data = brw->ds.prog_data;
   bool success;

   struct gl_tess_eval_program *dp = (struct gl_tess_eval_program *)prog;
   struct brw_tess_eval_program *bdp = brw_tess_eval_program(dp);

   memset(&key, 0, sizeof(key));

   brw_vue_setup_prog_key_for_precompile(ctx, &key.base, bdp->id, &dp->Base);

   /* Assume that the set of varyings coming in from the tessellation constrol
    * shader exactly matches what the tessellation evaluation shader requires.
    */
   key.input_varyings = dp->Base.InputsRead;

   success = do_ds_prog(brw, shader_prog, bdp, &key);

   brw->ds.base.prog_offset = old_prog_offset;
   brw->ds.prog_data = old_prog_data;

   return success;
}


bool
brw_ds_prog_data_compare(const void *in_a, const void *in_b)
{
   const struct brw_ds_prog_data *a = in_a;
   const struct brw_ds_prog_data *b = in_b;

   /* Compare the base structure. */
   if (!brw_stage_prog_data_compare(&a->base.base, &b->base.base))
      return false;

   /* Compare the rest of the struct. */
   const unsigned offset = sizeof(struct brw_stage_prog_data);
   if (memcmp(((char *) a) + offset, ((char *) b) + offset,
              sizeof(struct brw_ds_prog_data) - offset)) {
      return false;
   }

   return true;
}
