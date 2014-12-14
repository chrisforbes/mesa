/*
 * Copyright © 2014 Intel Corporation
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

#include "brw_context.h"
#include "brw_state.h"
#include "brw_defines.h"
#include "intel_batchbuffer.h"

static void
gen7_upload_hs_push_constants(struct brw_context *brw)
{
   struct brw_stage_state *stage_state = &brw->hs.base;
   /* BRW_NEW_TESS_CTRL_PROGRAM */
   const struct brw_tess_ctrl_program *hp =
      (struct brw_tess_ctrl_program *) brw->tess_ctrl_program;

   if (hp) {
      /* BRW_NEW_HS_PROG_DATA */
      const struct brw_stage_prog_data *prog_data = &brw->hs.prog_data->base.base;
      gen6_upload_push_constants(brw, &hp->program.Base, prog_data,
                                      stage_state, AUB_TRACE_VS_CONSTANTS);
   }

   gen7_upload_constant_state(brw, stage_state, hp, _3DSTATE_CONSTANT_HS);
}

const struct brw_tracked_state gen7_hs_push_constants = {
   .dirty = {
      .mesa  = _NEW_TRANSFORM |
               _NEW_PROGRAM_CONSTANTS,
      .brw   = BRW_NEW_BATCH |
               BRW_NEW_TESS_CTRL_PROGRAM |
               BRW_NEW_PUSH_CONSTANT_ALLOCATION |
               BRW_NEW_HS_PROG_DATA,
   },
   .emit = gen7_upload_hs_push_constants,
};


static void
upload_hs_state(struct brw_context *brw)
{
   const struct brw_stage_state *stage_state = &brw->hs.base;
   /* BRW_NEW_TESS_CTRL_PROGRAM */
   bool active = brw->tess_ctrl_program && brw->tess_eval_program;
   /* BRW_NEW_HS_PROG_DATA */
   const struct brw_vue_prog_data *prog_data = &brw->hs.prog_data->base;

   if (!brw->is_haswell && brw->gt == 2 && brw->hs.enabled != active)
      gen7_emit_cs_stall_flush(brw);

   if (active) {
      BEGIN_BATCH(7);
      OUT_BATCH(_3DSTATE_HS << 16 | (7 - 2));
      // XXX: 4.4, wrt max_hs_threads:
      /* A URB_FENCE command must be issued subsequent to any change to the value in this field and
         before any subsequent pipeline processing (e.g., via 3DPRIMITIVE or CONSTANT_BUFFER). See
         Graphics Processing Engine (Command Ordering Rules).*/
      //brw->state.dirty.brw |= BRW_NEW_URB_FENCE;
      OUT_BATCH(((ALIGN(stage_state->sampler_count, 4)/4) <<
                 GEN7_HS_SAMPLER_COUNT_SHIFT) |
                ((brw->hs.prog_data->base.base.binding_table.size_bytes / 4) <<
                 GEN7_HS_BINDING_TABLE_ENTRY_COUNT_SHIFT) |
                ((brw->max_hs_threads - 1) << GEN7_HS_MAX_THREADS_SHIFT));
      // We operate in simd4x2
      const int num_instances = (brw->hs.prog_data->instances + 1) / 2;
      assert(num_instances > 0);
      assert(num_instances <= 16);
      /* XXX: If the HS kernel uses a barrier function, software must restrict the Instance Count to the number of
       * threads that can be simultaneously active within a half-slice. Factors which must be considered
       * includes scratch memory availability.
       */
      OUT_BATCH(GEN7_HS_ENABLE |
                GEN7_HS_STATISTICS_ENABLE |
                ((num_instances - 1) <<
                 GEN7_HS_INSTANCE_CONTROL_SHIFT));
      OUT_BATCH(stage_state->prog_offset);
      if (brw->hs.prog_data->base.base.total_scratch) {
         OUT_RELOC(stage_state->scratch_bo,
                   I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                   ffs(brw->hs.prog_data->base.base.total_scratch) - 11);
      } else {
         OUT_BATCH(0);
      }

      uint32_t dw5 =
         GEN7_HS_INCLUDE_VERTEX_HANDLES |
         (prog_data->base.dispatch_grf_start_reg <<
          GEN7_HS_DISPATCH_START_GRF_SHIFT);

      /* semaphore URB handle */
      uint32_t dw6 = brw->urb.hs_semaphores_start * 128;

      OUT_BATCH(dw5);
      OUT_BATCH(dw6);
      ADVANCE_BATCH();
   } else {
      BEGIN_BATCH(7);
      OUT_BATCH(_3DSTATE_HS << 16 | (7 - 2));
      OUT_BATCH((0 << GEN7_HS_SAMPLER_COUNT_SHIFT) |
                (0 << GEN7_HS_BINDING_TABLE_ENTRY_COUNT_SHIFT) |
                (0 << GEN7_HS_MAX_THREADS_SHIFT));
      OUT_BATCH(GEN7_HS_STATISTICS_ENABLE);
      OUT_BATCH(0); /* prog_bo */
      OUT_BATCH(0); /* scratch space base offset */
      OUT_BATCH((1 << GEN7_HS_DISPATCH_START_GRF_SHIFT) |
                (0 << GEN7_HS_URB_READ_LENGTH_SHIFT) |
                GEN7_HS_INCLUDE_VERTEX_HANDLES |
                (0 << GEN7_HS_URB_ENTRY_READ_OFFSET_SHIFT));
      OUT_BATCH(0);
      ADVANCE_BATCH();
   }
   brw->hs.enabled = active;
}

const struct brw_tracked_state gen7_hs_state = {
   .dirty = {
      .mesa  = _NEW_TRANSFORM,
      .brw   = BRW_NEW_CONTEXT |
               BRW_NEW_TESS_CTRL_PROGRAM |
               BRW_NEW_BATCH |
               BRW_NEW_HS_PROG_DATA,
   },
   .emit = upload_hs_state,
};
