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
gen7_upload_ds_push_constants(struct brw_context *brw)
{
   struct brw_stage_state *stage_state = &brw->ds.base;
   /* BRW_NEW_TESS_EVAL_PROGRAM */
   const struct brw_tess_eval_program *dp =
      (struct brw_tess_eval_program *) brw->tess_eval_program;

   if (dp) {
      /* BRW_NEW_DS_PROG_DATA */
      const struct brw_stage_prog_data *prog_data = &brw->ds.prog_data->base.base;
      gen6_upload_push_constants(brw, &dp->program.Base, prog_data,
                                      stage_state, AUB_TRACE_VS_CONSTANTS);
   }

   gen7_upload_constant_state(brw, stage_state, dp, _3DSTATE_CONSTANT_DS);
}

const struct brw_tracked_state gen7_ds_push_constants = {
   .dirty = {
      .mesa  = _NEW_TRANSFORM |
               _NEW_PROGRAM_CONSTANTS,
      .brw   = BRW_NEW_BATCH |
               BRW_NEW_TESS_EVAL_PROGRAM |
               BRW_NEW_PUSH_CONSTANT_ALLOCATION |
               BRW_NEW_DS_PROG_DATA,
   },
   .emit = gen7_upload_ds_push_constants,
};


static void
upload_ds_state(struct brw_context *brw)
{
   const struct brw_stage_state *stage_state = &brw->ds.base;
   /* BRW_NEW_TESS_EVAL_PROGRAM */
   bool active = brw->tess_eval_program;
   if (active)
      assert(brw->tess_ctrl_program);
   /* BRW_NEW_DS_PROG_DATA */
   const struct brw_vue_prog_data *prog_data = &brw->ds.prog_data->base;

   // XXX: ivb gt2 gs state requires a cs stall flush here. is this true for ds, too?

   if (active) {
      BEGIN_BATCH(6);
      OUT_BATCH(_3DSTATE_DS << 16 | (6 - 2));
      OUT_BATCH(stage_state->prog_offset);
      OUT_BATCH(//single domain dispatch? probably not
                //vector mask enable? probably not
                ((ALIGN(stage_state->sampler_count, 4)/4) <<
                 GEN7_DS_SAMPLER_COUNT_SHIFT) |
                ((brw->ds.prog_data->base.base.binding_table.size_bytes / 4) <<
                 GEN7_DS_BINDING_TABLE_ENTRY_COUNT_SHIFT));
      if (brw->ds.prog_data->base.base.total_scratch) {
         OUT_RELOC(stage_state->scratch_bo,
                   I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                   ffs(brw->ds.prog_data->base.base.total_scratch) - 11);
      } else {
         OUT_BATCH(0);
      }
      uint32_t dw4 =
         (prog_data->base.dispatch_grf_start_reg <<
          GEN7_DS_DISPATCH_START_GRF_SHIFT) |
         (prog_data->urb_read_length <<
          GEN7_DS_URB_READ_LENGTH_SHIFT) |
         (0 << GEN7_DS_URB_ENTRY_READ_OFFSET_SHIFT);
      OUT_BATCH(dw4);

      uint32_t dw5 =
         ((brw->max_ds_threads - 1) << GEN7_DS_MAX_THREADS_SHIFT) |
         GEN7_DS_STATISTICS_ENABLE |
         (brw->ds.prog_data->domain == tri ? GEN7_DS_COMPUTE_W_COORDINATE_ENABLE : 0) |
         //ds cache disable?
         GEN7_DS_ENABLE;
      OUT_BATCH(dw5);
      ADVANCE_BATCH();
   } else {
      BEGIN_BATCH(6);
      OUT_BATCH(_3DSTATE_DS << 16 | (6 - 2));
      OUT_BATCH(0); /* prog_bo */
      OUT_BATCH((0 << GEN7_DS_SAMPLER_COUNT_SHIFT) |
                (0 << GEN7_DS_BINDING_TABLE_ENTRY_COUNT_SHIFT));
      OUT_BATCH(0); /* scratch space base offset */
      OUT_BATCH((1 << GEN7_DS_DISPATCH_START_GRF_SHIFT) |
                (0 << GEN7_DS_URB_READ_LENGTH_SHIFT) |
                (0 << GEN7_DS_URB_ENTRY_READ_OFFSET_SHIFT));
      OUT_BATCH(GEN7_DS_STATISTICS_ENABLE);
      ADVANCE_BATCH();
   }
   brw->ds.enabled = active;
}

const struct brw_tracked_state gen7_ds_state = {
   .dirty = {
      .mesa  = _NEW_TRANSFORM,
      .brw   = BRW_NEW_CONTEXT |
               BRW_NEW_TESS_EVAL_PROGRAM |
               BRW_NEW_DS_PROG_DATA |
               BRW_NEW_BATCH,
   },
   .emit = upload_ds_state,
};
