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
 * \file brw_vec4_ds_visitor.cpp
 *
 * Tessellaton evaluation shader specific code derived from the vec4_visitor class.
 */

#include "brw_vec4_ds_visitor.h"

const unsigned MAX_DS_INPUT_VERTICES = 32;

namespace brw {

vec4_ds_visitor::vec4_ds_visitor(struct brw_context *brw,
                                 struct brw_ds_compile *c,
                                 struct gl_shader_program *prog,
                                 struct brw_shader *shader,
                                 void *mem_ctx,
                                 bool no_spills)
   : vec4_visitor(brw, &c->base, &c->dp->program.Base, &c->key.base,
                  &c->prog_data.base, prog, MESA_SHADER_TESS_EVAL, mem_ctx,
                  INTEL_DEBUG & DEBUG_TS, no_spills,
                  ST_DS, ST_DS_WRITTEN, ST_DS_RESET),
     c(c)
{
}


dst_reg *
vec4_ds_visitor::make_reg_for_system_value(ir_variable *ir)
{
   dst_reg *reg = new(mem_ctx) dst_reg(this, ir->type);

   switch (ir->data.location) {
   case SYSTEM_VALUE_TESS_COORD:
      this->current_annotation = "initialize gl_TessCoord";
      emit(DS_OPCODE_GET_TESS_COORD, *reg);
      break;
   default:
      assert(!"not reached");
      break;
   }

   return reg;
}


int
vec4_ds_visitor::setup_varying_inputs(int payload_reg, int *attribute_map)
{
   /* For tessellation evaluation shaders there are N copies of the input attributes, where N
    * is the number of input vertices.  attribute_map[BRW_VARYING_SLOT_COUNT *
    * i + j] represents attribute j for vertex i.
    *
    * Note that DS inputs are read from the VUE 256 bits (2 vec4's) at a time,
    * so the total number of input slots that will be delivered to the DS (and
    * thus the stride of the input arrays) is urb_read_length * 2.
    */

   int num_patch_inputs = 0;

   num_patch_inputs = 0;
   for (int slot = 0; slot < c->input_vue_map.num_per_patch_slots; slot++) {
      const int varying = c->input_vue_map.slot_to_varying[slot];
      attribute_map[varying] = 2 * payload_reg + num_patch_inputs;
      printf("patch attrib slot %d: varying %d val %d\n",
            varying,
            varying,
            attribute_map[varying]);
      num_patch_inputs++;
   }

   assert(!(num_patch_inputs % 2));

   /*
    * XXX: Get this from the key!
    * XXX: This isnt even vaguely correct for ARB_sso with tess stages split between programs
    */
   struct gl_shader *tcs = this->shader_prog->_LinkedShaders[MESA_SHADER_TESS_CTRL];
   const unsigned num_vertex_inputs = tcs
      ? tcs->TessCtrl.VerticesOut
      : MAX_DS_INPUT_VERTICES;
   assert(num_vertex_inputs <= MAX_DS_INPUT_VERTICES);

   const unsigned input_array_stride = c->prog_data.base.urb_read_length * 2 - num_patch_inputs;

   for (int slot = c->input_vue_map.num_per_patch_slots; slot < c->input_vue_map.num_slots; slot++) {
      const int varying = c->input_vue_map.slot_to_varying[slot];
      for (unsigned vertex = 0; vertex < num_vertex_inputs; vertex++) {
         attribute_map[BRW_VARYING_SLOT_COUNT * (vertex + 1) + varying] =
            2 * payload_reg + num_patch_inputs + input_array_stride * vertex +
            slot - num_patch_inputs;
         printf("vert attrib slot %d: vertex %d varying %d val %d\n",
               BRW_VARYING_SLOT_COUNT * (vertex + 1) + varying,
               vertex,
               varying,
               attribute_map[BRW_VARYING_SLOT_COUNT * (vertex + 1) + varying]);
      }
   }

   //prog_data->urb_read_length = (nr_attributes + 1) / 2;

   //unsigned vue_entries =
      //MAX2(nr_attributes, prog_data->vue_map.num_slots);
   //prog_data->urb_entry_size = ALIGN(vue_entries, 4) / 4;

   int regs_used = (num_patch_inputs + 1) / 2;
   regs_used += (input_array_stride * num_vertex_inputs + 1) / 2;
   return payload_reg + regs_used;
}


void
vec4_ds_visitor::setup_payload()
{
   int attribute_map[BRW_VARYING_SLOT_COUNT * (MAX_DS_INPUT_VERTICES + 1)];

   /* If a tessellation evaluation shader tries to read from an input that wasn't written by
    * the tessellation control shader, that produces undefined results, but it shouldn't
    * crash anything.  So initialize attribute_map to zeros--that ensures that
    * these undefined results are read from r0.
    */
   memset(attribute_map, 0, sizeof(attribute_map));

   int reg = 0;

   /* The payload always contains important data in r0 and r1, which contains
    * the URB handles that are passed on to the URB write at the end
    * of the thread.
    */
   reg += 2;

   reg = setup_uniforms(reg);

   reg = setup_varying_inputs(reg, attribute_map);

   lower_attributes_to_hw_regs(attribute_map, true);

   this->first_non_payload_grf = reg;
}


void
vec4_ds_visitor::emit_prolog()
{
   /* If the tessellation evaluation shader uses the gl_PointSize input, we
    * need to fix it up to account for the fact that the tessellation control
    * shader stored it in the w component of VARYING_SLOT_PSIZ.
    */
   if (c->dp->program.Base.InputsRead & VARYING_BIT_PSIZ) {
      this->current_annotation = "swizzle gl_PointSize input";
      // XXX: for (int vertex = 0; vertex < c->hp->program.VerticesOut; vertex++) {
      for (int vertex = 0; vertex < 2; vertex++) {
         /* hack */
         dst_reg dst(ATTR,
                     BRW_VARYING_SLOT_COUNT * (vertex+1) + VARYING_SLOT_PSIZ);
         dst.type = BRW_REGISTER_TYPE_F;
         src_reg src(dst);
         dst.writemask = WRITEMASK_X;
         src.swizzle = BRW_SWIZZLE_WWWW;
         /*vec4_instruction *inst = */emit(MOV(dst, src));
      }
   }

   this->current_annotation = NULL;
}


void
vec4_ds_visitor::emit_urb_write_header(int mrf)
{
   /* No need to do anything for DS; an implied write to this MRF will be
    * performed by VS_OPCODE_URB_WRITE.
    */
   (void) mrf;
}


vec4_instruction *
vec4_ds_visitor::emit_urb_write_opcode(bool complete)
{
   /* For DS, the URB writes end the thread. */
   if (complete) {
      if (INTEL_DEBUG & DEBUG_SHADER_TIME)
         emit_shader_time_end();
   }

   vec4_instruction *inst = emit(VS_OPCODE_URB_WRITE);
   inst->urb_write_flags = complete ?
      BRW_URB_WRITE_EOT_COMPLETE : BRW_URB_WRITE_NO_FLAGS;

   return inst;
}


void
vec4_ds_visitor::emit_program_code()
{
   /* We don't support NV_tessellation_program. */
   assert(!"Unreached");
}


void
vec4_ds_visitor::emit_thread_end()
{
   /* For DS, we always end the thread by emitting a single vertex.
    * emit_urb_write_opcode() will take care of setting the eot flag on the
    * SEND instruction.
    */
   emit_vertex();
}


static const unsigned *
generate_assembly(struct brw_context *brw,
                  struct gl_shader_program *shader_prog,
                  struct gl_program *prog,
                  struct brw_vue_prog_data *prog_data,
                  void *mem_ctx,
                  const cfg_t *cfg,
                  unsigned *final_assembly_size)
{
   vec4_generator g(brw, shader_prog, prog, prog_data, mem_ctx,
                    INTEL_DEBUG & DEBUG_TS, "tess eval", "TES");
   return g.generate_assembly(cfg, final_assembly_size);
}

extern "C" const unsigned *
brw_ds_emit(struct brw_context *brw,
            struct gl_shader_program *prog,
            struct brw_ds_compile *c,
            void *mem_ctx,
            unsigned *final_assembly_size)
{
   struct brw_shader *shader =
      (brw_shader *) prog->_LinkedShaders[MESA_SHADER_TESS_EVAL];

   if (unlikely(INTEL_DEBUG & DEBUG_TS))
      brw_dump_ir("tessellation evaluation", prog, &shader->base, NULL);

   vec4_ds_visitor v(brw, c, prog, shader, mem_ctx, false /* no_spills */);
   if (!v.run()) {
      prog->LinkStatus = false;
      ralloc_strcat(&prog->InfoLog, v.fail_msg);
      return NULL;
   }

   return generate_assembly(brw, prog, &c->dp->program.Base, &c->prog_data.base,
                            mem_ctx, v.cfg, final_assembly_size);
   return NULL;
}


} /* namespace brw */
