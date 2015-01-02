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


/** patch header format is dependant on domain:
 * quad:    dw7-4 tess level outer xyzw
 *          dw3-2 tess level inner xy
 *          dw1-0 mbz
 * tri:     dw7-5 tess level outer xyz
 *          dw4   tess level inner x
 *          dw3-0 mbz
 * isoline: dw7-6 tess level outer xy
 *          dw5-0 mbz
 *
 * The hardware doesn't seem to mind some garbage data in the unused registers.
 */
void
vec4_hs_visitor::emit_tessellation_factors(struct brw_reg reg)
{
   current_annotation = "tessellation factors";
   vec4_instruction *inst;

   const src_reg src_outer = src_reg(output_reg[VARYING_SLOT_TESS_LEVEL_OUTER]);
   const src_reg src_inner = src_reg(output_reg[VARYING_SLOT_TESS_LEVEL_INNER]);

   struct brw_reg dst_upper = stride(suboffset(reg, 4), 0, 4, 1);
   struct brw_reg dst_lower = stride(suboffset(reg, 0), 0, 4, 1);

   GLenum prim_mode = ((struct brw_hs_prog_key *)key)->ds_primitive_mode;

   switch (prim_mode) {
   case GL_QUADS:
   case GL_ISOLINES:
      inst = emit(MOV(dst_upper,
                      swizzle(src_outer, BRW_SWIZZLE4(3, 2, 1, 0))));
      inst->force_writemask_all = true;

      inst = emit(MOV(dst_lower,
                      swizzle(src_inner, BRW_SWIZZLE4(3, 2, 1, 0))));
      inst->force_writemask_all = true;
      break;

   case GL_TRIANGLES:
      inst = emit(MOV(brw_writemask(dst_upper, WRITEMASK_YZW),
                      swizzle(src_outer, BRW_SWIZZLE4(3, 2, 1, 0))));
      inst->force_writemask_all = true;

      inst = emit(MOV(brw_writemask(dst_upper, WRITEMASK_X),
                      swizzle(src_inner, BRW_SWIZZLE_XXXX)));
      inst->force_writemask_all = true;
      break;

   default:
      unreachable("Bogus tessellation domain");
   }
}


void
vec4_hs_visitor::emit_urb_slot(int mrf, int sub_reg, int varying)
{
   dst_reg reg = dst_reg(MRF, mrf);
   reg.type = BRW_REGISTER_TYPE_F;
   struct brw_reg hw_reg = brw_message_reg(mrf);

   switch (varying) {
   case VARYING_SLOT_PSIZ:
      /* PSIZ is always in slot 0, and is coupled with other flags. */
      //FIXME
      hw_reg = stride(suboffset(hw_reg, sub_reg * 4), 0, 4, 1);
      current_annotation = "indices, point width, clip flags";
      emit_psiz_and_flags(hw_reg);
      break;
   case BRW_VARYING_SLOT_NDC:
      current_annotation = "NDC";
      emit(MOV(reg, src_reg(output_reg[BRW_VARYING_SLOT_NDC])));
      break;
   case VARYING_SLOT_POS:
      current_annotation = "gl_Position";
      emit(MOV(reg, src_reg(output_reg[VARYING_SLOT_POS])));
      break;
   case VARYING_SLOT_EDGE:
      /* This is present when doing unfilled polygons.  We're supposed to copy
       * the edge flag from the user-provided vertex array
       * (glEdgeFlagPointer), or otherwise we'll copy from the current value
       * of that attribute (starts as 1.0f).  This is then used in clipping to
       * determine which edges should be drawn as wireframe.
       */
      current_annotation = "edge flag";
      // XXX: HACK
      //emit(MOV(reg, src_reg(dst_reg(ATTR, VERT_ATTRIB_EDGEFLAG,
        //                            glsl_type::float_type, WRITEMASK_XYZW))));
      break;
   case BRW_VARYING_SLOT_PAD:
      /* No need to write to this slot */
      return;
   case VARYING_SLOT_TESS_LEVEL_OUTER:
   case VARYING_SLOT_TESS_LEVEL_INNER:
      assert(0);
      break;

   default:
      emit_generic_urb_slot(reg, varying);
      break;
   }

   // HACK
   hw_reg = stride(suboffset(hw_reg, sub_reg * 4), 0, 4, 1);
   vec4_instruction *inst = (vec4_instruction *)this->instructions.get_tail();
   inst->dst.file = HW_REG;
   inst->dst.fixed_hw_reg = hw_reg;
}


/**
 * Generates the VUE payload plus the necessary URB write instructions to
 * output it.
 *
 * XXX: Only send parts that were actually written by the thread.
 */
void
vec4_hs_visitor::emit_patch(const bool thread_end)
{
   /* MRF 0 is reserved for the debugger, so start with message header
    * in MRF 1.
    */
   int base_mrf = 1;
   int mrf = base_mrf;
   /* In the process of generating our URB write message contents, we
    * may need to unspill a register or load from an array.  Those
    * reads would use MRFs 14-15.
    * XXX: Is this true for >= gen7?
    */
   int max_usable_mrf = 13;

   /* First mrf is the g0-based message header containing URB handles and
    * such.
    */
   emit_urb_write_header(mrf++);

   /* We may need to split this up into several URB writes, so do them in a
    * loop.
    */
   int slot = 0;
   bool complete = false;
   do {
      /* URB offset is in URB row increments, and each of our MRFs is half of
       * one of those, since we're doing interleaved writes.
       * XXX
       */
      int offset = slot / 2;

      mrf = base_mrf + 1;

      if (slot == 0) {
         assert(prog_data->vue_map.slot_to_varying[0] ==
                VARYING_SLOT_TESS_LEVEL_INNER);
         assert(prog_data->vue_map.slot_to_varying[1] ==
                VARYING_SLOT_TESS_LEVEL_OUTER);
         emit_tessellation_factors(brw_message_reg(mrf++));
         slot += 2;
      }

      for (; slot < prog_data->vue_map.num_per_patch_slots; ++slot) {
         emit_urb_slot(mrf, slot % 2, prog_data->vue_map.slot_to_varying[slot]);

         if (slot % 2)
            mrf++;

         /* If this was max_usable_mrf, we can't fit anything more into this
          * URB WRITE.
          */
         if (mrf > max_usable_mrf) {
            slot++;
            break;
         }
      }

      assert(!(slot % 2));

      complete = (slot >= prog_data->vue_map.num_per_patch_slots) && thread_end;
      current_annotation = "URB write";
      vec4_instruction *inst = emit_urb_write_opcode(complete);
      inst->base_mrf = base_mrf;
      inst->mlen = mrf - base_mrf;
      //no need for padding -- we're not writing interleaved.
      //inst->mlen = align_interleaved_urb_mlen(brw, mrf - base_mrf);
      /*if ((inst->mlen % 2) != 1)
	 inst->mlen++;*/
      inst->offset += offset;
   } while(!complete);
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

   /* XXX: Use URB semaphore to determine whether this is the last invocation,
    * and only release the input vertices in that case. */

   unsigned input_vertices = ((brw_hs_prog_key *)key)->input_vertices;

   this->current_annotation = "release input vertices";
   for (unsigned i = 0; i < input_vertices; i += 2) {
      dst_reg r = dst_reg(this, glsl_type::uint_type);
      emit(HS_OPCODE_INPUT_RELEASE, r, src_reg(i));
   }

   emit_patch(true /* thread end */);
}


void
vec4_hs_visitor::emit_urb_write_header(int mrf)
{
   (void) mrf;
   return;
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
