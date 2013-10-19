/*
 * Copyright © 2010 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdint.h>
#include "brw_defines.h"
#include "glsl/ir.h"

#pragma once

enum register_file {
   BAD_FILE,
   GRF,
   MRF,
   IMM,
   HW_REG, /* a struct brw_reg */
   ATTR,
   UNIFORM, /* prog_data->params[reg] */
};

#ifdef __cplusplus

class backend_instruction : public exec_node {
public:
   bool is_tex();
   bool is_math();
   bool is_control_flow();
   bool can_do_source_mods();

   enum opcode opcode; /* BRW_OPCODE_* or FS_OPCODE_* */

   uint32_t predicate;
   bool predicate_inverse;
};

class backend_visitor : public ir_visitor {
public:

   struct brw_context *brw;
   struct gl_context *ctx;
   struct brw_shader *shader;
   struct gl_shader_program *shader_prog;
   struct gl_program *prog;
   struct brw_stage_prog_data *stage_prog_data;

   /** ralloc context for temporary data used during compile */
   void *mem_ctx;

   /**
    * List of either fs_inst or vec4_instruction (inheriting from
    * backend_instruction)
    */
   exec_list instructions;

   virtual void dump_instruction(backend_instruction *inst) = 0;
   void dump_instructions();

   void assign_common_binding_table_offsets(uint32_t next_binding_table_offset);
};

uint32_t brw_texture_offset(struct gl_context *ctx, ir_constant *offset);

#endif /* __cplusplus */

int brw_type_for_base_type(const struct glsl_type *type);
uint32_t brw_conditional_for_comparison(unsigned int op);
uint32_t brw_math_function(enum opcode op);
const char *brw_instruction_name(enum opcode op);
