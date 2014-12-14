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
 * \file brw_vec4_hs_visitor.h
 *
 * Tessellation-control-shader-specific code derived from the vec4_visitor class.
 */

#ifndef BRW_VEC4_HS_VISITOR_H
#define BRW_VEC4_HS_VISITOR_H

#include "brw_vec4.h"


struct brw_hs_prog_key
{
   struct brw_vue_prog_key base;

   GLbitfield64 input_varyings;
   GLuint input_vertices;
   GLenum ds_primitive_mode;
};


/**
 * Scratch data used when compiling a GLSL tessellation control shader
 */
struct brw_hs_compile
{
   struct brw_vec4_compile base;
   struct brw_hs_prog_key key;
   struct brw_hs_prog_data prog_data;
   struct brw_vue_map input_vue_map;

   struct brw_tess_ctrl_program *hp;
};

#ifdef __cplusplus
extern "C" {
#endif

const unsigned *brw_hs_emit(struct brw_context *brw,
                            struct gl_shader_program *prog,
                            struct brw_hs_compile *c,
                            void *mem_ctx,
                            unsigned *final_assembly_size);

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef __cplusplus
namespace brw {

class vec4_hs_visitor : public vec4_visitor
{
public:
   vec4_hs_visitor(struct brw_context *brw,
                   struct brw_hs_compile *c,
                   struct gl_shader_program *prog,
                   void *mem_ctx,
                   bool no_spills);

protected:
   virtual dst_reg *make_reg_for_system_value(ir_variable *ir);
   virtual void setup_payload();
   virtual void emit_prolog();
   virtual void emit_program_code();
   virtual void emit_thread_end();
   virtual void emit_urb_write_header(int mrf);
   virtual vec4_instruction *emit_urb_write_opcode(bool complete);
   virtual int compute_array_stride(ir_dereference_array *ir);
   virtual void emit_tessellation_factors(struct brw_reg reg);
   virtual void emit_patch(const bool thread_end);
   virtual void emit_urb_slot(int mrf, int sub_reg, int varying);

   virtual void visit(ir_barrier *);

private:
   src_reg vertex_count;
   const struct brw_hs_compile * const c;
};

} /* namespace brw */
#endif /* __cplusplus */

#endif /* BRW_VEC4_HS_VISITOR_H */
