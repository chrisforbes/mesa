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
   case SYSTEM_VALUE_INVOCATION_ID:
      this->current_annotation = "initialize gl_InvocationID";
      emit(HS_OPCODE_GET_INSTANCE_ID, *reg);
      break;
   case SYSTEM_VALUE_PRIMITIVE_ID:
      this->current_annotation = "primitive id";
      emit(HS_OPCODE_GET_PRIMITIVE_ID, *reg);
      break;
   default:
      assert(!"not reached");
      break;
   }

   return reg;
}


void
vec4_hs_visitor::setup_payload()
{
   int reg = 0;

   /* The payload always contains important data in r0, which contains
    * the URB handles that are passed on to the URB write at the end
    * of the thread.
    */
   reg++;

   /* r1.0 - r4.7 may contain the input control point URB handles,
    * which we use to pull vertex data.
    */

   //int num_vertices = ((brw_hs_prog_key *)key)->input_vertices;
   //int vertex_handle_regs = (num_vertices + 7) / 8;
   //reg += vertex_handle_regs;
   reg += 4;

   /* Push constants may start at r5.0 */

   reg = setup_uniforms(reg);

   this->first_non_payload_grf = reg;
}


void
vec4_hs_visitor::emit_prolog()
{
   /* If the tessellation control shader uses the gl_PointSize input, we need to fix it up
    * to account for the fact that the vertex shader stored it in the w
    * component of VARYING_SLOT_PSIZ.
    */

   /* HS threads are dispatched with the dispatch mask set to 0xFFFF.
    * If there are an odd number of output vertices, then the final
    * HS instance dispatched will only have its bottom half doing real
    * work, and so we need to disable the upper half:
    */
   int num_instances = ((brw_hs_prog_data *)prog_data)->instances;

   if (num_instances % 2) {
      dst_reg instance_id = dst_reg(this, glsl_type::ivec4_type);
      emit(HS_OPCODE_GET_INSTANCE_ID, instance_id);
      emit(CMP(dst_null_d(), swizzle(src_reg(instance_id), BRW_SWIZZLE_XXXX),
               src_reg(num_instances),
               BRW_CONDITIONAL_L));
      /* (Matching ENDIF is in shader epilog) */
      emit(IF(BRW_PREDICATE_NORMAL));
   }

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
   /* If we had an odd number of invocations, the prolog wrapped the
    * body of the shader in an IF to adjust the execution mask. Emit the
    * matching ENDIF.
    */
   int num_instances = ((brw_hs_prog_data *)prog_data)->instances;

   if (num_instances % 2) {
      emit(BRW_OPCODE_ENDIF);
   }

   /* Release input vertices */

   if (num_instances > 2) {
      this->current_annotation = "check if last thread to complete";
      dst_reg val = dst_reg(this, glsl_type::uint_type);
      emit(HS_OPCODE_URB_SEMAPHORE_INCR, val);
      emit(CMP(dst_null_d(), swizzle(src_reg(val), BRW_SWIZZLE_XXXX),
               src_reg((uint32_t)((num_instances+1)/2 - 1)),
               BRW_CONDITIONAL_EQ));
      emit(IF(BRW_PREDICATE_NORMAL));
   }

   unsigned input_vertices = ((brw_hs_prog_key *)key)->input_vertices;

   this->current_annotation = "release input vertices";

   for (unsigned i = 0; i < input_vertices; i += 2) {
      dst_reg r = dst_reg(this, glsl_type::uint_type);
      bool is_unpaired = (i == input_vertices - 1) && (input_vertices & 1);
      emit(HS_OPCODE_INPUT_RELEASE, r, src_reg(i), src_reg((unsigned)is_unpaired));
   }

   vec4_instruction *inst = emit(VS_OPCODE_URB_WRITE);
   inst->mlen = 1;   /* just the header, no data. */
   inst->urb_write_flags = BRW_URB_WRITE_EOT_COMPLETE;

   if (num_instances > 2) {
      /* re-zero the URB semaphore */
      this->current_annotation = "reset urb semaphore";
      dst_reg scratch = dst_reg(this, glsl_type::uint_type);
      emit(HS_OPCODE_URB_SEMAPHORE_RESET, scratch);
      emit(BRW_OPCODE_ENDIF);
   }
}


void
vec4_hs_visitor::visit(ir_barrier *ir)
{
   /* XXX: Emit code to send BarrierMsg to the Message Gateway shared function */
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
                    INTEL_DEBUG & DEBUG_TS, "tess ctrl", "TCS");
   return g.generate_assembly(cfg, final_assembly_size);
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
      brw_dump_ir("tessellation control", prog, &shader->base, NULL);

   vec4_hs_visitor v(brw, c, prog, mem_ctx, false /* no_spills */);
   if (!v.run()) {
      prog->LinkStatus = false;
      ralloc_strcat(&prog->InfoLog, v.fail_msg);
      return NULL;
   }

   return generate_assembly(brw, prog, &c->hp->program.Base, &c->prog_data.base,
                            mem_ctx, v.cfg, final_assembly_size);
}


} /* namespace brw */
