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
 * \file brw_vec4_hs.c
 *
 * State atom for client-programmable tessellation control shaders,
 * and support code.
 */

#include "brw_vec4_hs.h"
#include "brw_context.h"
#include "brw_vec4_hs_visitor.h"
#include "brw_state.h"


static bool
do_hs_prog(struct brw_context *brw,
           struct gl_shader_program *prog,
           struct brw_tess_ctrl_program *hp,
           struct brw_hs_prog_key *key)
{
   struct brw_stage_state *stage_state = &brw->hs.base;
   struct brw_hs_compile c;
   memset(&c, 0, sizeof(c));
   c.key = *key;
   c.hp = hp;

   const int patch_vertices_out = hp->program.VerticesOut;
   c.prog_data.instances = patch_vertices_out;
   c.prog_data.uses_barrier_function = false; // XXX: gp->program.UsesBarrier;

   c.prog_data.include_primitive_id =
      (hp->program.Base.InputsRead & VARYING_BIT_PRIMITIVE_ID) != 0;

   /* Allocate the references to the uniforms that will end up in the
    * prog_data associated with the compiled program, and which will be freed
    * by the state cache.
    *
    * Note: param_count needs to be num_uniform_components * 4, since we add
    * padding around uniform values below vec4 size, so the worst case is that
    * every uniform is a float which gets padded to the size of a vec4.
    */
   struct gl_shader *hs = prog->_LinkedShaders[MESA_SHADER_TESS_CTRL];
   int param_count = hs->num_uniform_components * 4;

   /* We also upload clip plane data as uniforms */
   //param_count += MAX_CLIP_PLANES * 4;

   c.prog_data.base.base.param =
      rzalloc_array(NULL, const gl_constant_value *, param_count);
   c.prog_data.base.base.pull_param =
      rzalloc_array(NULL, const gl_constant_value *, param_count);
   /* Setting nr_params here NOT to the size of the param and pull_param
    * arrays, but to the number of uniform components vec4_visitor
    * needs. vec4_visitor::setup_uniforms() will set it back to a proper value.
    */
   c.prog_data.base.base.nr_params = ALIGN(param_count, 4) / 4 + hs->num_samplers;

   GLbitfield64 outputs_written = hp->program.Base.OutputsWritten;

   brw_compute_tess_vue_map(brw, &c.prog_data.base.vue_map, outputs_written,
                            hp->program.IsPatch);

   /* Compute URB entry size.  The maximum allowed URB entry size is 32k.
    * That divides up as follows:
    *
    *     32 bytes for the patch header (tessellation factors)
    * XXX 32 bytes tessellation factors (in case DS wants to know them)
    *    480 bytes for per-patch varyings (a varying component is 4 bytes and
    *              gl_MaxTessPatchComponents = 120)
    *  16384 bytes for per-vertex varyings (a varying component is 4 bytes,
    *              gl_MaxPatchVertices = 32 and
    *              gl_MaxTessControlOutputComponents = 128)
    *
    *  15840 bytes left for varying packing overhead
    */
   const int num_per_patch_slots = c.prog_data.base.vue_map.num_per_patch_slots;
   const int num_per_vertex_slots = c.prog_data.base.vue_map.num_per_vertex_slots;
   unsigned output_size_bytes = 32;
   // XXX: if (((struct brw_tess_eval_program *) brw->tess_eval_program)->program.Base.InputsRead & BITFIELD64_BIT(VARYING_SLOT_TESS_LEVEL_OUTER | VARYING_SLOT_TESS_LEVEL_INNER))
      //output_size_bytes += 32;
   output_size_bytes += num_per_patch_slots * 16;
   output_size_bytes += patch_vertices_out * num_per_vertex_slots * 16;

   assert(output_size_bytes >= 1);
   if (output_size_bytes > GEN7_MAX_HS_URB_ENTRY_SIZE_BYTES)
      return false;

   /* URB entry sizes are stored as a multiple of 64 bytes. */
   c.prog_data.base.urb_entry_size = ALIGN(output_size_bytes, 64) / 64;

   brw_compute_vue_map(brw, &c.input_vue_map, c.key.input_varyings);

   /* HS does not use the usual payload pushing from URB to GRFs,
    * because we don't have enough registers for a full-size payload, and
    * the hardware is broken on Haswell anyway. */
   c.prog_data.base.urb_read_length = 0;

   void *mem_ctx = ralloc_context(NULL);
   unsigned program_size;
   const unsigned *program =
      brw_hs_emit(brw, prog, &c, mem_ctx, &program_size);
   if (program == NULL) {
      ralloc_free(mem_ctx);
      return false;
   }

   /* Scratch space is used for register spilling */
   if (c.base.last_scratch) {
      perf_debug("Tessellation Control shader triggered register spilling.  "
                 "Try reducing the number of live vec4 values to "
                 "improve performance.\n");

      c.prog_data.base.base.total_scratch
         = brw_get_scratch_size(c.base.last_scratch*REG_SIZE);

      brw_get_scratch_bo(brw, &stage_state->scratch_bo,
			 c.prog_data.base.base.total_scratch * brw->max_hs_threads);
   }

   brw_upload_cache(&brw->cache, BRW_HS_PROG,
                    &c.key, sizeof(c.key),
                    program, program_size,
                    &c.prog_data, sizeof(c.prog_data),
                    &stage_state->prog_offset, &brw->hs.prog_data);
   ralloc_free(mem_ctx);

   return true;
}


static void
brw_upload_hs_prog(struct brw_context *brw)
{
   struct gl_context *ctx = &brw->ctx;
   struct brw_stage_state *stage_state = &brw->hs.base;
   struct brw_hs_prog_key key;
   /* BRW_NEW_TESS_CTRL_PROGRAM */
   struct brw_tess_ctrl_program *hp =
      (struct brw_tess_ctrl_program *) brw->tess_ctrl_program;

   if (hp == NULL) {
      /* No tessellation control shader.
       * Vertex data just passes straight through.
       */
      if (brw->state.dirty.brw & BRW_NEW_VUE_MAP_VS) {
         brw->vue_map_hs_out = brw->vue_map_vs;
         brw->state.dirty.brw |= BRW_NEW_VUE_MAP_HS_OUT;
      }

      /* Other state atoms had better not try to access prog_data, since
       * there's no HS program.
       */
      brw->hs.prog_data = NULL;
      brw->hs.base.prog_data = NULL;

      return;
   }

   struct gl_program *prog = &hp->program.Base;

   memset(&key, 0, sizeof(key));

   key.base.program_string_id = hp->id;
   brw_setup_vec4_key_clip_info(brw, &key.base,
                                hp->program.Base.UsesClipDistanceOut);

   /* _NEW_LIGHT | _NEW_BUFFERS */
   key.base.clamp_vertex_color = ctx->Light._ClampVertexColor;

   /* _NEW_TEXTURE */
   brw_populate_sampler_prog_key_data(ctx, prog, stage_state->sampler_count,
                                      &key.base.tex);

   /* BRW_NEW_VUE_MAP_VS */
   key.input_varyings = brw->vue_map_vs.slots_valid;

   if (!brw_search_cache(&brw->cache, BRW_HS_PROG,
                         &key, sizeof(key),
                         &stage_state->prog_offset, &brw->hs.prog_data)) {
      bool success =
         do_hs_prog(brw, ctx->Shader.CurrentProgram[MESA_SHADER_TESS_CTRL], hp,
                    &key);
      assert(success);
   }
   brw->hs.base.prog_data = &brw->hs.prog_data->base.base;

   if (memcmp(&brw->vs.prog_data->base.vue_map, &brw->vue_map_hs_out,
              sizeof(brw->vue_map_hs_out)) != 0) {
      brw->vue_map_hs_out = brw->hs.prog_data->base.vue_map;
      brw->state.dirty.brw |= BRW_NEW_VUE_MAP_HS_OUT;
   }
}


const struct brw_tracked_state brw_hs_prog = {
   .dirty = {
      .mesa  = (_NEW_LIGHT | _NEW_BUFFERS | _NEW_TEXTURE),
      .brw   = BRW_NEW_TESS_CTRL_PROGRAM | BRW_NEW_VUE_MAP_VS,
   },
   .emit = brw_upload_hs_prog
};


bool
brw_hs_precompile(struct gl_context *ctx, struct gl_shader_program *prog)
{
   struct brw_context *brw = brw_context(ctx);
   struct brw_hs_prog_key key;
   uint32_t old_prog_offset = brw->hs.base.prog_offset;
   struct brw_hs_prog_data *old_prog_data = brw->hs.prog_data;
   bool success;

   if (!prog->_LinkedShaders[MESA_SHADER_TESS_CTRL])
      return true;

   struct gl_tess_ctrl_program *hp = (struct gl_tess_ctrl_program *)
      prog->_LinkedShaders[MESA_SHADER_TESS_CTRL]->Program;
   struct brw_tess_ctrl_program *bhp = brw_tess_ctrl_program(hp);

   memset(&key, 0, sizeof(key));

   brw_vec4_setup_prog_key_for_precompile(ctx, &key.base, bhp->id, &hp->Base);

   /* Assume that the set of varyings coming in from the vertex shader exactly
    * matches what the tessellation control shader requires.
    */
   key.input_varyings = hp->Base.InputsRead;

   success = do_hs_prog(brw, prog, bhp, &key);

   brw->hs.base.prog_offset = old_prog_offset;
   brw->hs.prog_data = old_prog_data;

   return success;
}


bool
brw_hs_prog_data_compare(const void *in_a, const void *in_b)
{
   const struct brw_hs_prog_data *a = in_a;
   const struct brw_hs_prog_data *b = in_b;

   /* Compare the base structure. */
   if (!brw_stage_prog_data_compare(&a->base.base, &b->base.base))
      return false;

   /* Compare the rest of the struct. */
   const unsigned offset = sizeof(struct brw_stage_prog_data);
   if (memcmp(((char *) a) + offset, ((char *) b) + offset,
              sizeof(struct brw_hs_prog_data) - offset)) {
      return false;
   }

   return true;
}
