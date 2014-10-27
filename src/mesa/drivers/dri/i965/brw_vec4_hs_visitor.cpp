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

   printf("Push constants start at r%d.0\n", reg);

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
   /* XXX: needs to go in key */

   int num_vertices = ((brw_hs_prog_key *)key)->input_vertices;

//   if (c->hp->program.Base.InputsRead & VARYING_BIT_PSIZ) {
  //    this->current_annotation = "swizzle gl_PointSize input";
    //  for (int vertex = 0; vertex < num_vertices; vertex++) {
      //   dst_reg dst(ATTR,
        //             BRW_VARYING_SLOT_COUNT * vertex + VARYING_SLOT_PSIZ);
//         dst.type = BRW_REGISTER_TYPE_F;
  //       src_reg src(dst);
   //      dst.writemask = WRITEMASK_X;
     //    src.swizzle = BRW_SWIZZLE_WWWW;
  //       /*inst = */emit(MOV(dst, src));
    //  }
//   }

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

   /* XXX: this needs to go via key, if we have to specialize the program on it */
   GLenum prim_mode;
   if (shader_prog->_LinkedShaders[MESA_SHADER_TESS_EVAL])
      prim_mode = shader_prog->_LinkedShaders[MESA_SHADER_TESS_EVAL]->TessEval.PrimitiveMode;
   else
      prim_mode = GL_TRIANGLES;

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

      // XXX: pass unused tessellation levels if tessellation evaluation shader reads them back.
      /*inst = emit(MOV(brw_writemask(dst_lower, WRITEMASK_X),
                      swizzle(src_outer, BRW_SWIZZLE4(3, 2, 1, 0))));
      inst->force_writemask_all = true;
      inst = emit(MOV(brw_writemask(dst_lower, WRITEMASK_ZW),
                      swizzle(src_inner, BRW_SWIZZLE4(3, 2, 1, 0))));
      inst->force_writemask_all = true;*/
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

   /* Release input vertices */

   /* XXX: Use URB semaphore to determine whether this is the last invocation,
    * and only release the input vertices in that case. */
   /* XXX: this is a dumb way to get the shader */
   struct gl_shader *tcs = this->shader_prog->_LinkedShaders[MESA_SHADER_TESS_CTRL];
   assert(tcs->TessCtrl.VerticesOut == 1);

   int input_vertices = ((brw_hs_prog_key *)key)->input_vertices;

   for (int i = 0; i < input_vertices; i += 2) {
      printf("Release verts %d,%d\n", i, i+1);
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

static const unsigned *
generate_assembly(struct brw_context *brw,
                  struct gl_shader_program *shader_prog,
                  struct gl_program *prog,
                  struct brw_vec4_prog_data *prog_data,
                  void *mem_ctx,
                  const cfg_t *cfg,
                  unsigned *final_assembly_size)
{
   vec4_generator g(brw, shader_prog, prog, prog_data, mem_ctx,
                    INTEL_DEBUG & DEBUG_TS, MESA_SHADER_TESS_CTRL);
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
