/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
 * All Rights Reserved.
 * Copyright 2009 VMware, Inc.  All Rights Reserved.
 * Copyright © 2010-2011 Intel Corporation
 * Copyright © 2014 Fabian Bieler
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

// XXX: add comment

// XXX: what about user defined varyings from vertex to tess eval shader?
#include "glsl_symbol_table.h"
#include "ir.h"
#include "ir_builder.h"
#include "glsl_parser_extras.h"
#include "ir_optimization.h"

using namespace ir_builder;


/** State used to build the fragment program:
 */
class uniform_tess_ctrl : public ir_factory {
public:
   struct gl_shader *shader;
   exec_list *top_instructions;
};


static void
emit_instructions(uniform_tess_ctrl *p)
{
   ir_variable *dst, *src;
   static const char *const varying[] = {
      "gl_Position", "gl_PointSize", "gl_ClipDistance"
   };

   dst = p->shader->symbols->get_variable("gl_out");
   src = p->shader->symbols->get_variable("gl_in");
   ir_variable *const invocation_id =
      p->shader->symbols->get_variable("gl_InvocationID");

   for (int i = 0; i < 3; ++i) {
      ir_dereference_variable *invocation_id_deref =
         new(p->mem_ctx) ir_dereference_variable(invocation_id);
      ir_dereference_array *dst_array_deref =
         new(p->mem_ctx) ir_dereference_array(dst, invocation_id_deref);
      ir_dereference_record *dst_record_deref =
         new(p->mem_ctx) ir_dereference_record(dst_array_deref, varying[i]);

      invocation_id_deref =
         new(p->mem_ctx) ir_dereference_variable(invocation_id);
      ir_dereference_array *src_array_deref =
         new(p->mem_ctx) ir_dereference_array(src, invocation_id_deref);
      ir_dereference_record *src_record_deref =
         new(p->mem_ctx) ir_dereference_record(src_array_deref, varying[i]);

      p->emit(assign(dst_record_deref, src_record_deref));
   }

   dst = p->shader->symbols->get_variable("gl_TessLevelOuter");
   src = p->shader->symbols->get_variable("PatchDefaultOuterLevel");
   p->emit(assign(dst, src));
   dst = p->shader->symbols->get_variable("gl_TessLevelInner");
   src = p->shader->symbols->get_variable("PatchDefaultInnerLevel");
   p->emit(assign(dst, src));
}


static void
add_uniform(uniform_tess_ctrl *p, const char *const name, const int dim)
{
   const glsl_type *const float_array_t =
      glsl_type::get_array_instance(glsl_type::float_type, dim);

   ir_variable *const var = new(p->mem_ctx) ir_variable(float_array_t, name,
                                                        ir_var_uniform);

   var->data.read_only = true;
   var->data.location = -1;
   var->data.explicit_location = 0;
   var->data.explicit_index = 0;

   p->instructions->push_tail(var);
   p->shader->symbols->add_variable(var);
}


/**
 * Generate a simple pass-through tessellation control shader that assigns
 * gl_TessLevelOuter and -Inner from uniforms.
 */
struct gl_shader *
create_ff_tess_ctrl_program(struct gl_context *ctx)
{
   uniform_tess_ctrl p;
   _mesa_glsl_parse_state *state;

   p.mem_ctx = ralloc_context(NULL);
   p.shader = ctx->Driver.NewShader(ctx, 0, GL_TESS_CONTROL_SHADER);
   p.shader->ir = new(p.shader) exec_list;
   state = new(p.shader) _mesa_glsl_parse_state(ctx, MESA_SHADER_TESS_CTRL,
                                                p.shader);
   // XXX: is memmory context correct?

   p.shader->symbols = state->symbols;
   p.top_instructions = p.shader->ir;
   p.instructions = p.shader->ir;
   //p.shader_program = ctx->Driver.NewShaderProgram(ctx, 0);

   state->language_version = 150;//<----------------+
   state->es_shader = false;//                      |
   state->ARB_tessellation_shader_enable = true;//--+
   _mesa_glsl_initialize_types(state);
   _mesa_glsl_initialize_variables(p.instructions, state);

   add_uniform(&p, "PatchDefaultOuterLevel", 4);
   add_uniform(&p, "PatchDefaultInnerLevel", 2);

   ir_variable *const gl_out = p.shader->symbols->get_variable("gl_out");
   gl_out->type = glsl_type::get_array_instance(gl_out->type->fields.array,
                                                state->Const.MaxPatchVertices);

   ir_function *main_f = new(p.mem_ctx) ir_function("main");
   p.emit(main_f);
   state->symbols->add_function(main_f);

   ir_function_signature *main_sig =
      new(p.mem_ctx) ir_function_signature(p.shader->symbols->get_type("void"));
   main_sig->is_defined = true;
   main_f->add_signature(main_sig);

   p.instructions = &main_sig->body;
   emit_instructions(&p);

   validate_ir_tree(p.shader->ir);

   const struct gl_shader_compiler_options *options =
      &ctx->Const.ShaderCompilerOptions[MESA_SHADER_TESS_CTRL];

   while (do_common_optimization(p.shader->ir, false, false, options,
                                 ctx->Const.NativeIntegers))
      ;
   reparent_ir(p.shader->ir, p.shader->ir);

   p.shader->CompileStatus = true;
   p.shader->Version = state->language_version;
   p.shader->uses_builtin_functions = state->uses_builtin_functions;
   p.shader->TessCtrl.VerticesOut = state->Const.MaxPatchVertices;

   ralloc_free(p.mem_ctx);
   return p.shader;
}

