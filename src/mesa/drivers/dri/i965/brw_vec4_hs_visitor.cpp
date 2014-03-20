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
 * \file brw_vec4_hs_visitor.cpp
 *
 * Tessellaton control shader specific code derived from the vec4_visitor class.
 */

#include "brw_vec4_hs_visitor.h"

const unsigned MAX_HS_INPUT_VERTICES = 32;

namespace brw {

vec4_hs_visitor::vec4_hs_visitor(struct brw_context *brw,
                                 struct brw_hs_compile *c,
                                 struct gl_shader_program *prog,
                                 struct brw_shader *shader,
                                 void *mem_ctx,
                                 bool no_spills)
   : vec4_visitor(brw, &c->base, &c->hp->program.Base, &c->key.base,
                  &c->prog_data.base, prog, MESA_SHADER_TESS_CTRL, mem_ctx,
                  INTEL_DEBUG & DEBUG_TS, no_spills,
                  ST_HS, ST_HS_WRITTEN, ST_HS_RESET),
     c(c)
{
}


dst_reg *
vec4_hs_visitor::make_reg_for_system_value(ir_variable *ir)
{
   dst_reg *reg = new(mem_ctx) dst_reg(this, ir->type);

   switch (ir->data.location) {
   default:
      assert(!"not reached");
      break;
   }

   return reg;
}


int
vec4_hs_visitor::setup_varying_inputs(int payload_reg, int *attribute_map)
{
   /* For tessellation control shaders there are N copies of the input attributes, where N
    * is the number of input vertices per patch.  attribute_map[BRW_VARYING_SLOT_COUNT *
    * i + j] represents attribute j for vertex i.
    *
    * Note that HS inputs are read from the VUE 256 bits (2 vec4's) at a time,
    * so the total number of input slots that will be delivered to the HS (and
    * thus the stride of the input arrays) is urb_read_length * 2.
    */
   const unsigned num_input_vertices = 3;// XXX
   assert(num_input_vertices <= MAX_HS_INPUT_VERTICES);
   unsigned input_array_stride = c->prog_data.base.urb_read_length * 2;

   for (int slot = 0; slot < c->input_vue_map.num_slots; slot++) {
      int varying = c->input_vue_map.slot_to_varying[slot];
      for (unsigned vertex = 0; vertex < num_input_vertices; vertex++) {
         attribute_map[BRW_VARYING_SLOT_COUNT * vertex + varying] =
            2 * payload_reg + input_array_stride * vertex +
            slot;
      }
   }

   int regs_used = (input_array_stride * num_input_vertices + 1) / 2;
   return payload_reg + regs_used;
}


void
vec4_hs_visitor::setup_payload()
{
   int attribute_map[BRW_VARYING_SLOT_COUNT * MAX_HS_INPUT_VERTICES];

   /* If a tessellation control shader tries to read from an input that wasn't written by
    * the vertex shader, that produces undefined results, but it shouldn't
    * crash anything.  So initialize attribute_map to zeros--that ensures that
    * these undefined results are read from r0.
    */
   memset(attribute_map, 0, sizeof(attribute_map));

   int reg = 0;

   /* The payload always contains important data in r0, which contains
    * the URB handles that are passed on to the URB write at the end
    * of the thread.
    */
   reg++;

   reg = setup_uniforms(reg);

   reg = setup_varying_inputs(reg, attribute_map);

   // XXX: hw seems to load first attribute in high half of register
   lower_attributes_to_hw_regs(attribute_map, true);

   this->first_non_payload_grf = reg;
}


void
vec4_hs_visitor::emit_prolog()
{
   /* If the tessellation control shader uses the gl_PointSize input, we need to fix it up
    * to account for the fact that the vertex shader stored it in the w
    * component of VARYING_SLOT_PSIZ.
    */
   if (c->hp->program.Base.InputsRead & VARYING_BIT_PSIZ) {
      this->current_annotation = "swizzle gl_PointSize input";
      for (int vertex = 0; vertex < 3/* XXX */; vertex++) {
         dst_reg dst(ATTR,
                     BRW_VARYING_SLOT_COUNT * vertex + VARYING_SLOT_PSIZ);
         dst.type = BRW_REGISTER_TYPE_F;
         src_reg src(dst);
         dst.writemask = WRITEMASK_X;
         src.swizzle = BRW_SWIZZLE_WWWW;
         /*inst = */emit(MOV(dst, src));
      }
   }

   // XXX: modify dispatch in simd4x2 mode for last thread with uneven number of output vertices?
   // 4.7.1:
   /* HS threads will be dispatched with the dispatch mask set to 0xFFFF. It is the responsibility of the kernel to
      modify the execution mask as required (e.g., if operating in SIMD4x2 mode but only the lower half is
      active, as would happen in one thread is the threads were computing an odd number of OCPs via
      SIMD4x2 operation).*/
   // set execution mask here or only write mask right before send? what about side effects (atomic counters, image store)?

   this->current_annotation = NULL;
}


void
vec4_hs_visitor::emit_program_code()
{
   /* We don't support NV_tessellation_program. */
   assert(!"Unreached");
}


void
vec4_hs_visitor::emit_thread_end()
{
   // XXX: not good: emit_vertex thinks we are using interleaved write.
   emit_vertex();
}


void
vec4_hs_visitor::emit_urb_write_header(int mrf)
{
   /* The SEND instruction that writes the vertex data to the VUE will use
    * per_slot_offset=true, which means that DWORD 3 of the message
    * header specifies an offset (in multiples of 256 bits) into the URB entry
    * at which the write should take place.
    *
    * So we have to prepare a message header with the appropriate offset
    * values.
    */
   (void) mrf;
   return;
#if 0
   dst_reg mrf_reg(MRF, mrf);
   src_reg r0(retype(brw_vec8_grf(0, 0), BRW_REGISTER_TYPE_UD));
   this->current_annotation = "URB write header";
   vec4_instruction *inst = emit(MOV(mrf_reg, r0));
   inst->force_writemask_all = true;
   emit(HS_OPCODE_SET_WRITE_OFFSET, mrf_reg, this->vertex_count,
        (uint32_t) c->prog_data.output_vertex_size_hwords);
#endif
}

vec4_instruction *
vec4_hs_visitor::emit_urb_write_opcode(bool complete)
{
   /* For HS, the URB writes end the thread. */
   if (complete) {
      if (INTEL_DEBUG & DEBUG_SHADER_TIME)
         emit_shader_time_end();
   }

   vec4_instruction *inst = emit(HS_OPCODE_URB_WRITE); // XXX: can use VS opcode here?

   inst->urb_write_flags = BRW_URB_WRITE_USE_CHANNEL_MASKS |
                           (complete ? BRW_URB_WRITE_EOT_COMPLETE :
                                       BRW_URB_WRITE_NO_FLAGS);

   return inst;
}


int
vec4_hs_visitor::compute_array_stride(ir_dereference_array *ir)
{
   /* Tessellation control shader inputs and outputs are arrays XXX except per-patch outputs, with the expected array layout:
    * all shader inputs are interleaved into
    * one giant array.  At this stage of compilation, we assume that the
    * stride of the array is BRW_VARYING_SLOT_COUNT.  Later,
    * setup_attributes() XXX: setup_varying_inputs? will remap our accesses to the actual input array.
    */
   ir_dereference_variable *deref_var = ir->array->as_dereference_variable();
   if (deref_var && deref_var->var->data.mode == ir_var_shader_in)
      return BRW_VARYING_SLOT_COUNT;
   else if (deref_var && deref_var->var->data.mode == ir_var_shader_out &&
            !deref_var->var->data.patch)
      return BRW_VARYING_SLOT_COUNT;
   else
      return vec4_visitor::compute_array_stride(ir);
}

#if 0
void
vec4_hs_visitor::visit(ir_barrier *)
{
}
#endif

static const unsigned *
generate_assembly(struct brw_context *brw,
                  struct gl_shader_program *shader_prog,
                  struct gl_program *prog,
                  struct brw_vec4_prog_data *prog_data,
                  void *mem_ctx,
                  exec_list *instructions,
                  unsigned *final_assembly_size)
{
   vec4_generator g(brw, shader_prog, prog, prog_data, mem_ctx,
                    INTEL_DEBUG & DEBUG_TS);
   return g.generate_assembly(instructions, final_assembly_size);
}

extern "C" const unsigned *
brw_hs_emit(struct brw_context *brw,
            struct gl_shader_program *prog,
            struct brw_hs_compile *c,
            void *mem_ctx,
            unsigned *final_assembly_size)
{
   struct brw_shader *shader =
      (brw_shader *) prog->_LinkedShaders[MESA_SHADER_TESS_CTRL];

   if (unlikely(INTEL_DEBUG & DEBUG_TS))
      brw_dump_ir(brw, "tessellation control", prog, &shader->base, NULL);

   vec4_hs_visitor v(brw, c, prog, shader, mem_ctx, false /* no_spills */);
   if (!v.run()) {
      prog->LinkStatus = false;
      ralloc_strcat(&prog->InfoLog, v.fail_msg);
      return NULL;
   }

   return generate_assembly(brw, prog, &c->hp->program.Base, &c->prog_data.base,
                            mem_ctx, &v.instructions, final_assembly_size);
}


} /* namespace brw */
