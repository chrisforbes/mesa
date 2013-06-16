/*
 * Copyright © 2011 Intel Corporation
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

#include <assert.h>

#include "intel_batchbuffer.h"
#include "intel_fbo.h"
#include "intel_mipmap_tree.h"

#include "brw_context.h"
#include "brw_defines.h"
#include "brw_state.h"

#include "brw_blorp.h"
#include "gen5_blorp.h"

/**
 * \name Constants for BLORP VBO
 * \{
 */
#define GEN5_BLORP_NUM_VERTICES 3
#define GEN5_BLORP_NUM_VUE_ELEMS 8
#define GEN5_BLORP_VBO_SIZE (GEN5_BLORP_NUM_VERTICES \
                             * GEN5_BLORP_NUM_VUE_ELEMS \
                             * sizeof(float))
/** \} */


/**
 * CMD_STATE_BASE_ADDRESS
 *
 * From the Ironlake PRM, Volume 1, Part 1, Table STATE_BASE_ADDRESS:
 *     The following commands must be reissued following any change to the
 *     base addresses:
 *         3DSTATE_PIPELINE_POINTERS
 *         3DSTATE_BINDING_TABLE_POINTERS
 *         MEDIA_STATE_POINTERS
 */
void
gen5_blorp_emit_state_base_address(struct brw_context *brw,
                                   const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(8);
   OUT_BATCH(CMD_STATE_BASE_ADDRESS << 16 | (8 - 2));
   OUT_BATCH(1); /* GeneralStateBaseAddressModifyEnable */
   /* SurfaceStateBaseAddress */
   OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_SAMPLER, 0, 1);
   OUT_BATCH(1); /* IndirectObjectBaseAddress */
   if (params->use_wm_prog) {
      OUT_RELOC(brw->cache.bo, I915_GEM_DOMAIN_INSTRUCTION, 0,
                1); /* Instruction base address: shader kernels */
   } else {
      OUT_BATCH(1); /* InstructionBaseAddress */
   }
   OUT_BATCH(0xfffff001); /* GeneralStateUpperBound */
   OUT_BATCH(1); /* IndirectObjectUpperBound*/
   OUT_BATCH(1); /* InstructionAccessUpperBound */
   ADVANCE_BATCH();
}


/* TODO: merge back with gen6_blorp_emit_vertices; almost identical.
 *    - BRW_VB0_PITCH_SHIFT != GEN6_VB0_PITCH_SHIFT
 * */
void
gen5_blorp_emit_vertices(struct brw_context *brw,
                         const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;
   uint32_t vertex_offset;

   /* Setup VBO for the rectangle primitive..
    *
    * A rectangle primitive (3DPRIM_RECTLIST) consists of only three
    * vertices. The vertices reside in screen space with DirectX coordinates
    * (that is, (0, 0) is the upper left corner).
    *
    *   v2 ------ implied
    *    |        |
    *    |        |
    *   v0 ----- v1
    *
    * Since the VS is disabled, the clipper loads each VUE directly from
    * the URB. This is controlled by the 3DSTATE_VERTEX_BUFFERS and
    * 3DSTATE_VERTEX_ELEMENTS packets below. The VUE contents are as follows:
    *   dw0: Reserved, MBZ.
    *   dw1: Render Target Array Index. The HiZ op does not use indexed
    *        vertices, so set the dword to 0.
    *   dw2: Viewport Index. The HiZ op disables viewport mapping and
    *        scissoring, so set the dword to 0.
    *   dw3: Point Width: The HiZ op does not emit the POINTLIST primitive, so
    *        set the dword to 0.
    *   dw4: Vertex Position X.
    *   dw5: Vertex Position Y.
    *   dw6: Vertex Position Z.
    *   dw7: Vertex Position W.
    *
    * [same as SNB]
    * For details, see the Sandybridge PRM, Volume 2, Part 1, Section 1.5.1
    * "Vertex URB Entry (VUE) Formats".
    */
   {
      float *vertex_data;

      const float vertices[GEN5_BLORP_VBO_SIZE] = {
         /* v0 */ 0, 0, 0, 0,     (float) params->x0, (float) params->y1, 0, 1,
         /* v1 */ 0, 0, 0, 0,     (float) params->x1, (float) params->y1, 0, 1,
         /* v2 */ 0, 0, 0, 0,     (float) params->x0, (float) params->y0, 0, 1,
      };

      vertex_data = (float *) brw_state_batch(brw, AUB_TRACE_VERTEX_BUFFER,
                                              GEN5_BLORP_VBO_SIZE, 32,
                                              &vertex_offset);
      memcpy(vertex_data, vertices, GEN5_BLORP_VBO_SIZE);
   }

   /* 3DSTATE_VERTEX_BUFFERS */
   {
      const int num_buffers = 1;
      const int batch_length = 1 + 4 * num_buffers;

      uint32_t dw0 = BRW_VB0_ACCESS_VERTEXDATA |
                     (GEN5_BLORP_NUM_VUE_ELEMS * sizeof(float)) << BRW_VB0_PITCH_SHIFT;   /* HERE */

      BEGIN_BATCH(batch_length);
      OUT_BATCH((_3DSTATE_VERTEX_BUFFERS << 16) | (batch_length - 2));
      OUT_BATCH(dw0);
      /* start address */
      OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_VERTEX, 0,
		vertex_offset);
      /* end address */
      OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_VERTEX, 0,
		vertex_offset + GEN5_BLORP_VBO_SIZE - 1);
      OUT_BATCH(0);
      ADVANCE_BATCH();
   }

   /* 3DSTATE_VERTEX_ELEMENTS
    *
    * Fetch dwords 0 - 7 from each VUE. See the comments above where
    * the vertex_bo is filled with data.
    */
   {
      const int num_elements = 2;
      const int batch_length = 1 + 2 * num_elements;

      BEGIN_BATCH(batch_length);
      OUT_BATCH((_3DSTATE_VERTEX_ELEMENTS << 16) | (batch_length - 2));
      /* Element 0 */
      OUT_BATCH(BRW_VE0_VALID |  /* HERE */
                BRW_SURFACEFORMAT_R32G32B32A32_FLOAT << BRW_VE0_FORMAT_SHIFT |
                0 << BRW_VE0_SRC_OFFSET_SHIFT);
      OUT_BATCH(BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_0_SHIFT |
                BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_1_SHIFT |
                BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_2_SHIFT |
                BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_3_SHIFT);
      /* Element 1 */
      OUT_BATCH(BRW_VE0_VALID |  /* HERE */
                BRW_SURFACEFORMAT_R32G32B32A32_FLOAT << BRW_VE0_FORMAT_SHIFT |
                16 << BRW_VE0_SRC_OFFSET_SHIFT);
      OUT_BATCH(BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_0_SHIFT |
                BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_1_SHIFT |
                BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_2_SHIFT |
                BRW_VE1_COMPONENT_STORE_SRC << BRW_VE1_COMPONENT_3_SHIFT);
      ADVANCE_BATCH();
   }
}


/* CMD_URB_FENCE, CMD_CS_URB_STATE
 *
 * Assign the entire URB to the VS. Even though the VS disabled, URB space
 * is still needed because the clipper loads the VUE's from the URB. From
 * the Sandybridge PRM, Volume 2, Part 1, Section 3DSTATE,
 * Dword 1.15:0 "VS Number of URB Entries":
 *     This field is always used (even if VS Function Enable is DISABLED).
 *
 * The warning below appears in the PRM (Section 3DSTATE_URB), but we can
 * safely ignore it because this batch contains only one draw call.
 *     Because of URB corruption caused by allocating a previous GS unit
 *     URB entry to the VS unit, software is required to send a “GS NULL
 *     Fence” (Send URB fence with VS URB size == 1 and GS URB size == 0)
 *     plus a dummy DRAW call before any case where VS will be taking over
 *     GS URB space.
 */
static void
gen5_blorp_emit_urb_config(struct brw_context *brw,
                           const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   /* Note: CMD_URB_FENCE can't cross a 64byte cacheline boundary, so we emit
    * MI_NOOP if we would */

   if ((brw->intel.batch.used & 15) > 12) {
      int pad = 16 - (brw->intel.batch.used & 15);
      do
         brw->intel.batch.map[brw->intel.batch.used++] = MI_NOOP;
      while (--pad);
   }

   BEGIN_BATCH(5);
   OUT_BATCH(CMD_URB_FENCE << 16 | (3-2) | 0x3f00);   /* vs|gs|clp|sf|vfe|cs */
   OUT_BATCH(brw->urb.max_vs_entries);                /* the whole URB to VS */
   OUT_BATCH(0);
   ADVANCE_BATCH();

   BEGIN_BATCH(2);
   OUT_BATCH(CMD_CS_URB_STATE << 16 | (2-2));
   OUT_BATCH(0);
   ADVANCE_BATCH();
}


/* COLOR_CALC_STATE */
static uint32_t
gen5_blorp_emit_cc_unit_state(struct brw_context *brw,
                            const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   uint32_t cc_unit_state_offset;
   uint32_t cc_vp_offset;

   struct brw_cc_unit_state *cc;
   struct brw_cc_viewport *ccv;

   /* CC_VP_STATE */
   ccv = (struct brw_cc_viewport *)
      brw_state_batch(brw, AUB_TRACE_CC_VP_STATE,
                      sizeof(struct brw_cc_viewport), 32,
                      &cc_vp_offset);
   ccv->min_depth = 0.0;
   ccv->max_depth = 1.0;

   /* CC_STATE */
   cc = (struct brw_cc_unit_state *)
      brw_state_batch(brw, AUB_TRACE_CC_STATE,
                      sizeof(struct brw_cc_unit_state), 64,
                      &cc_unit_state_offset);

   memset(cc, 0, sizeof(*cc));

   cc->cc6.clamp_post_alpha_blend = 1;
   cc->cc6.clamp_pre_alpha_blend = 1;
   cc->cc6.clamp_range = BRW_RENDERTARGET_CLAMPRANGE_FORMAT;

   /* NOTE: pre-Gen6, color mask lives in WM_STATE */

   /* When blitting from an XRGB source to a ARGB destination, we need to
    * interpret the missing channel as 1.0.  Blending can do that for us:
    * we simply use the RGB values from the fragment shader ("source RGB"),
    * but smash the alpha channel to 1.
    */
   if (params->src.mt &&
       _mesa_get_format_bits(params->dst.mt->format, GL_ALPHA_BITS) > 0 &&
       _mesa_get_format_bits(params->src.mt->format, GL_ALPHA_BITS) == 0) {
      cc->cc3.blend_enable = 1;
      cc->cc3.ia_blend_enable = 1;

      cc->cc6.blend_function = BRW_BLENDFUNCTION_ADD;
      cc->cc5.ia_blend_function = BRW_BLENDFUNCTION_ADD;

      cc->cc6.src_blend_factor = BRW_BLENDFACTOR_SRC_COLOR;
      cc->cc6.dest_blend_factor = BRW_BLENDFACTOR_ZERO;
      cc->cc5.ia_src_blend_factor = BRW_BLENDFACTOR_ONE;
      cc->cc5.ia_dest_blend_factor = BRW_BLENDFACTOR_ZERO;
   }

   /* depth */
   cc->cc2.depth_write_enable = 1;
   if (params->hiz_op == GEN6_HIZ_OP_DEPTH_RESOLVE) {
      cc->cc2.depth_test = 1;
      cc->cc2.depth_test_function = COMPAREFUNC_NEVER;
   }

   /* viewport state pointer and relocation */
   cc->cc4.cc_viewport_state_offset = (intel->batch.bo->offset +
         brw->cc.vp_offset) >> 5;   /* reloc */

   drm_intel_bo_emit_reloc(brw->intel.batch.bo,
         (cc_unit_state_offset +
         offsetof(struct brw_cc_unit_state, cc4)),
         intel->batch.bo, cc_vp_offset,
         I915_GEM_DOMAIN_INSTRUCTION, 0);

   return cc_unit_state_offset;
}

/* ALL FIXED TO HERE ---- */

/* WM push constants */
/*
uint32_t
gen5_blorp_emit_wm_constants(struct brw_context *brw,
                             const brw_blorp_params *params)
{
   uint32_t wm_push_const_offset;

   void *constants = brw_state_batch(brw, AUB_TRACE_WM_CONSTANTS,
                                     sizeof(params->wm_push_consts),
                                     32, &wm_push_const_offset);
   memcpy(constants, &params->wm_push_consts,
          sizeof(params->wm_push_consts));

   return wm_push_const_offset;
}
*/

/* BINDING_TABLE.  See brw_wm_binding_table(). */
/*
uint32_t
gen6_blorp_emit_binding_table(struct brw_context *brw,
                              const brw_blorp_params *params,
                              uint32_t wm_surf_offset_renderbuffer,
                              uint32_t wm_surf_offset_texture)
{
   uint32_t wm_bind_bo_offset;
   uint32_t *bind = (uint32_t *)
      brw_state_batch(brw, AUB_TRACE_BINDING_TABLE,
                      sizeof(uint32_t) *
                      BRW_BLORP_NUM_BINDING_TABLE_ENTRIES,
                      32, // alignment
                      &wm_bind_bo_offset);
   bind[BRW_BLORP_RENDERBUFFER_BINDING_TABLE_INDEX] =
      wm_surf_offset_renderbuffer;
   bind[BRW_BLORP_TEXTURE_BINDING_TABLE_INDEX] = wm_surf_offset_texture;

   return wm_bind_bo_offset;
}
*/


/**
 * SAMPLER_STATE.  See brw_update_sampler_state().
 */
/*
static uint32_t
gen6_blorp_emit_sampler_state(struct brw_context *brw,
                              const brw_blorp_params *params)
{
   uint32_t sampler_offset;

   struct brw_sampler_state *sampler = (struct brw_sampler_state *)
      brw_state_batch(brw, AUB_TRACE_SAMPLER_STATE,
                      sizeof(struct brw_sampler_state),
                      32, &sampler_offset);
   memset(sampler, 0, sizeof(*sampler));

   sampler->ss0.min_filter = BRW_MAPFILTER_LINEAR;
   sampler->ss0.mip_filter = BRW_MIPFILTER_NONE;
   sampler->ss0.mag_filter = BRW_MAPFILTER_LINEAR;

   sampler->ss1.r_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
   sampler->ss1.s_wrap_mode = BRW_TEXCOORDMODE_CLAMP;
   sampler->ss1.t_wrap_mode = BRW_TEXCOORDMODE_CLAMP;

   sampler->ss0.min_mag_neq = 1;

   // Set LOD bias: 
   sampler->ss0.lod_bias = 0;

   sampler->ss0.lod_preclamp = 1; // OpenGL mode 
   sampler->ss0.default_color_mode = 0; // OpenGL/DX10 mode 

   // Set BaseMipLevel, MaxLOD, MinLOD: 
    *
    * XXX: I don't think that using firstLevel, lastLevel works,
    * because we always setup the surface state as if firstLevel ==
    * level zero.  Probably have to subtract firstLevel from each of
    * these:
    //
   sampler->ss0.base_level = U_FIXED(0, 1);

   sampler->ss1.max_lod = U_FIXED(0, 6);
   sampler->ss1.min_lod = U_FIXED(0, 6);

   sampler->ss3.non_normalized_coord = 1;

   sampler->ss3.address_round |= BRW_ADDRESS_ROUNDING_ENABLE_U_MIN |
      BRW_ADDRESS_ROUNDING_ENABLE_V_MIN |
      BRW_ADDRESS_ROUNDING_ENABLE_R_MIN;
   sampler->ss3.address_round |= BRW_ADDRESS_ROUNDING_ENABLE_U_MAG |
      BRW_ADDRESS_ROUNDING_ENABLE_V_MAG |
      BRW_ADDRESS_ROUNDING_ENABLE_R_MAG;

   return sampler_offset;
}
*/

/**
 * 3DSTATE_SAMPLER_STATE_POINTERS.  See upload_sampler_state_pointers().
 */
/*
static void
gen6_blorp_emit_sampler_state_pointers(struct brw_context *brw,
                                       const brw_blorp_params *params,
                                       uint32_t sampler_offset)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(4);
   OUT_BATCH(_3DSTATE_SAMPLER_STATE_POINTERS << 16 |
             VS_SAMPLER_STATE_CHANGE |
             GS_SAMPLER_STATE_CHANGE |
             PS_SAMPLER_STATE_CHANGE |
             (4 - 2));
   OUT_BATCH(0); //VS
   OUT_BATCH(0); //GS
   OUT_BATCH(sampler_offset);
   ADVANCE_BATCH();
}
*/


/* VS_STATE
 *
 * Disable vertex shader.
 */
static uint32_t
gen5_blorp_emit_vs_disable(struct brw_context *brw,
                           const brw_blorp_params *params)
{
   struct brw_vs_unit_state *vs;
   uint32_t vs_offset;

   vs = (struct brw_vs_unit_state *)brw_state_batch(brw, AUB_TRACE_VS_STATE,
         sizeof(*vs), 32, &vs_offset);
   memset(vs, 0, sizeof(*vs));
   return vs_offset;
}


/* CLIP_STATE
 *
 * Disable the clipper.
 */
static uint32_t
gen5_blorp_emit_clip_disable(struct brw_context *brw,
                             const brw_blorp_params *params)
{
   struct brw_clip_unit_state *clip;
   uint32_t clip_offset;

   clip = (struct brw_clip_unit_state *)brw_state_batch(brw, AUB_TRACE_CLIP_STATE,
         sizeof(*clip), 32, &clip_offset);
   memset(clip, 0, sizeof(*clip));

   clip->clip5.clip_mode = BRW_CLIPMODE_ACCEPT_ALL;

   return clip_offset;
}


/* SF_STATE
 */
static uint32_t
gen5_blorp_emit_sf_config(struct brw_context *brw,
                          const brw_blorp_params *params)
{
   assert(!"undone");
   return 0;
}


/* WM_STATE
 */
static uint32_t
gen5_blorp_emit_wm_config(struct brw_context *brw,
                          const brw_blorp_params *params)
{
   assert(!"undone");
   return 0;
}


/* 3DSTATE_PIPELINED_POINTERS */
static void
gen5_blorp_emit_pipelined_pointers(struct brw_context *brw,
                                   const brw_blorp_params *params,
                                   uint32_t vs_offset,/* uint32_t gs_offset,*/
                                   uint32_t clip_offset, uint32_t sf_offset,
                                   uint32_t wm_offset, uint32_t cc_state_offset)
{
   struct intel_context *intel = &brw->intel;

   /* do we need a w/a flush here, or is the one done earlier good enough? */

   BEGIN_BATCH(7);
   OUT_BATCH(_3DSTATE_PIPELINED_POINTERS << 16 | (7-2));
   OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, vs_offset);
   OUT_BATCH(0);  /* gs */
   OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, clip_offset | 1);
   OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, sf_offset);
   OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, wm_offset);
   OUT_RELOC(intel->batch.bo, I915_GEM_DOMAIN_INSTRUCTION, 0, cc_state_offset);
   ADVANCE_BATCH();
}


/* 3DSTATE_SF
 *
 * Disable ViewportTransformEnable (dw2.1)
 *
 * From the SandyBridge PRM, Volume 2, Part 1, Section 1.3, "3D
 * Primitives Overview":
 *     RECTLIST: Viewport Mapping must be DISABLED (as is typical with the
 *     use of screen- space coordinates).
 *
 * A solid rectangle must be rendered, so set FrontFaceFillMode (dw2.4:3)
 * and BackFaceFillMode (dw2.5:6) to SOLID(0).
 *
 * From the Sandy Bridge PRM, Volume 2, Part 1, Section
 * 6.4.1.1 3DSTATE_SF, Field FrontFaceFillMode:
 *     SOLID: Any triangle or rectangle object found to be front-facing
 *     is rendered as a solid object. This setting is required when
 *     (rendering rectangle (RECTLIST) objects.
 */
/*
static void
gen6_blorp_emit_sf_config(struct brw_context *brw,
                          const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(20);
   OUT_BATCH(_3DSTATE_SF << 16 | (20 - 2));
   OUT_BATCH((1 - 1) << GEN6_SF_NUM_OUTPUTS_SHIFT | // only position
             1 << GEN6_SF_URB_ENTRY_READ_LENGTH_SHIFT |
             0 << GEN6_SF_URB_ENTRY_READ_OFFSET_SHIFT);
   OUT_BATCH(0); //dw2
   OUT_BATCH(params->num_samples > 1 ? GEN6_SF_MSRAST_ON_PATTERN : 0);
   for (int i = 0; i < 16; ++i)
      OUT_BATCH(0);
   ADVANCE_BATCH();
}*/


/**
 * Enable or disable thread dispatch and set the HiZ op appropriately.
 */
/*
static void
gen6_blorp_emit_wm_config(struct brw_context *brw,
                          const brw_blorp_params *params,
                          uint32_t prog_offset,
                          brw_blorp_prog_data *prog_data)
{
   struct intel_context *intel = &brw->intel;
   uint32_t dw2, dw4, dw5, dw6;

   // Even when thread dispatch is disabled, max threads (dw5.25:31) must be
    * nonzero to prevent the GPU from hanging. See the valid ranges in the
    * BSpec, Volume 2a.11 Windower, Section 3DSTATE_WM, Dword 5.25:31
    * "Maximum Number Of Threads".
    *
    * To be safe (and to minimize extraneous code) we go ahead and fully
    * configure the WM state whether or not there is a WM program.
    //

   dw2 = dw4 = dw5 = dw6 = 0;
   switch (params->hiz_op) {
   case GEN6_HIZ_OP_DEPTH_CLEAR:
      dw4 |= GEN6_WM_DEPTH_CLEAR;
      break;
   case GEN6_HIZ_OP_DEPTH_RESOLVE:
      dw4 |= GEN6_WM_DEPTH_RESOLVE;
      break;
   case GEN6_HIZ_OP_HIZ_RESOLVE:
      dw4 |= GEN6_WM_HIERARCHICAL_DEPTH_RESOLVE;
      break;
   case GEN6_HIZ_OP_NONE:
      break;
   default:
      assert(0);
      break;
   }
   dw5 |= GEN6_WM_LINE_AA_WIDTH_1_0;
   dw5 |= GEN6_WM_LINE_END_CAP_AA_WIDTH_0_5;
   dw5 |= (brw->max_wm_threads - 1) << GEN6_WM_MAX_THREADS_SHIFT;
   dw6 |= 0 << GEN6_WM_BARYCENTRIC_INTERPOLATION_MODE_SHIFT; // No interp
   dw6 |= 0 << GEN6_WM_NUM_SF_OUTPUTS_SHIFT; // No inputs from SF
   if (params->use_wm_prog) {
      dw2 |= 1 << GEN6_WM_SAMPLER_COUNT_SHIFT; // Up to 4 samplers
      dw4 |= prog_data->first_curbe_grf << GEN6_WM_DISPATCH_START_GRF_SHIFT_0;
      dw5 |= GEN6_WM_16_DISPATCH_ENABLE;
      dw5 |= GEN6_WM_KILL_ENABLE; // TODO: temporarily smash on
      dw5 |= GEN6_WM_DISPATCH_ENABLE; // We are rendering
   }

   if (params->num_samples > 1) {
      dw6 |= GEN6_WM_MSRAST_ON_PATTERN;
      if (prog_data && prog_data->persample_msaa_dispatch)
         dw6 |= GEN6_WM_MSDISPMODE_PERSAMPLE;
      else
         dw6 |= GEN6_WM_MSDISPMODE_PERPIXEL;
   } else {
      dw6 |= GEN6_WM_MSRAST_OFF_PIXEL;
      dw6 |= GEN6_WM_MSDISPMODE_PERSAMPLE;
   }

   BEGIN_BATCH(9);
   OUT_BATCH(_3DSTATE_WM << 16 | (9 - 2));
   OUT_BATCH(params->use_wm_prog ? prog_offset : 0);
   OUT_BATCH(dw2);
   OUT_BATCH(0); // No scratch needed
   OUT_BATCH(dw4);
   OUT_BATCH(dw5);
   OUT_BATCH(dw6);
   OUT_BATCH(0); // No other programs
   OUT_BATCH(0); // No other programs
   ADVANCE_BATCH();
}
*/

/*
static void
gen6_blorp_emit_constant_ps(struct brw_context *brw,
                            const brw_blorp_params *params,
                            uint32_t wm_push_const_offset)
{
   struct intel_context *intel = &brw->intel;

   // Make sure the push constants fill an exact integer number of
    * registers.
    //
   assert(sizeof(brw_blorp_wm_push_constants) % 32 == 0);

   // There must be at least one register worth of push constant data. //
   assert(BRW_BLORP_NUM_PUSH_CONST_REGS > 0);

   // Enable push constant buffer 0. //
   BEGIN_BATCH(5);
   OUT_BATCH(_3DSTATE_CONSTANT_PS << 16 |
             GEN6_CONSTANT_BUFFER_0_ENABLE |
             (5 - 2));
   OUT_BATCH(wm_push_const_offset + (BRW_BLORP_NUM_PUSH_CONST_REGS - 1));
   OUT_BATCH(0);
   OUT_BATCH(0);
   OUT_BATCH(0);
   ADVANCE_BATCH();
}*/

/*
static void
gen6_blorp_emit_constant_ps_disable(struct brw_context *brw,
                                    const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(5);
   OUT_BATCH(_3DSTATE_CONSTANT_PS << 16 | (5 - 2));
   OUT_BATCH(0);
   OUT_BATCH(0);
   OUT_BATCH(0);
   OUT_BATCH(0);
   ADVANCE_BATCH();
}o*/

/**
 * 3DSTATE_BINDING_TABLE_POINTERS
 */
/*
static void
gen6_blorp_emit_binding_table_pointers(struct brw_context *brw,
                                       const brw_blorp_params *params,
                                       uint32_t wm_bind_bo_offset)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(4);
   OUT_BATCH(_3DSTATE_BINDING_TABLE_POINTERS << 16 |
             GEN6_BINDING_TABLE_MODIFY_PS |
             (4 - 2));
   OUT_BATCH(0); //vs
   OUT_BATCH(0); //gs
   OUT_BATCH(wm_bind_bo_offset); //wm/ps
   ADVANCE_BATCH();
}*/

/*
static void
gen6_blorp_emit_depth_stencil_config(struct brw_context *brw,
                                     const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;
   struct gl_context *ctx = &intel->ctx;
   uint32_t draw_x = params->depth.x_offset;
   uint32_t draw_y = params->depth.y_offset;
   uint32_t tile_mask_x, tile_mask_y;

   brw_get_depthstencil_tile_masks(params->depth.mt,
                                   params->depth.level,
                                   params->depth.layer,
                                   NULL,
                                   &tile_mask_x, &tile_mask_y);

   // 3DSTATE_DEPTH_BUFFER
   {
      uint32_t tile_x = draw_x & tile_mask_x;
      uint32_t tile_y = draw_y & tile_mask_y;
      uint32_t offset =
         intel_region_get_aligned_offset(params->depth.mt->region,
                                         draw_x & ~tile_mask_x,
                                         draw_y & ~tile_mask_y, false);

      // According to the Sandy Bridge PRM, volume 2 part 1, pp326-327
       * (3DSTATE_DEPTH_BUFFER dw5), in the documentation for "Depth
       * Coordinate Offset X/Y":
       *
       *   "The 3 LSBs of both offsets must be zero to ensure correct
       *   alignment"
       *
       * We have no guarantee that tile_x and tile_y are correctly aligned,
       * since they are determined by the mipmap layout, which is only aligned
       * to multiples of 4.
       *
       * So, to avoid hanging the GPU, just smash the low order 3 bits of
       * tile_x and tile_y to 0.  This is a temporary workaround until we come
       * up with a better solution.
       //
      WARN_ONCE((tile_x & 7) || (tile_y & 7),
                "Depth/stencil buffer needs alignment to 8-pixel boundaries.\n"
                "Truncating offset, bad rendering may occur.\n");
      tile_x &= ~7;
      tile_y &= ~7;

      intel_emit_post_sync_nonzero_flush(intel);
      intel_emit_depth_stall_flushes(intel);

      BEGIN_BATCH(7);
      OUT_BATCH(_3DSTATE_DEPTH_BUFFER << 16 | (7 - 2));
      OUT_BATCH((params->depth.mt->region->pitch - 1) |
                params->depth_format << 18 |
                1 << 21 | // separate stencil enable //
                1 << 22 | // hiz enable //
                BRW_TILEWALK_YMAJOR << 26 |
                1 << 27 | // y-tiled //
                BRW_SURFACE_2D << 29);
      OUT_RELOC(params->depth.mt->region->bo,
                I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                offset);
      OUT_BATCH(BRW_SURFACE_MIPMAPLAYOUT_BELOW << 1 |
                (params->depth.width + tile_x - 1) << 6 |
                (params->depth.height + tile_y - 1) << 19);
      OUT_BATCH(0);
      OUT_BATCH(tile_x |
                tile_y << 16);
      OUT_BATCH(0);
      ADVANCE_BATCH();
   }

   // 3DSTATE_HIER_DEPTH_BUFFER
   {
      struct intel_region *hiz_region = params->depth.mt->hiz_mt->region;
      uint32_t hiz_offset =
         intel_region_get_aligned_offset(hiz_region,
                                         draw_x & ~tile_mask_x,
                                         (draw_y & ~tile_mask_y) / 2, false);

      BEGIN_BATCH(3);
      OUT_BATCH((_3DSTATE_HIER_DEPTH_BUFFER << 16) | (3 - 2));
      OUT_BATCH(hiz_region->pitch - 1);
      OUT_RELOC(hiz_region->bo,
                I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER,
                hiz_offset);
      ADVANCE_BATCH();
   }

   // 3DSTATE_STENCIL_BUFFER
   {
      BEGIN_BATCH(3);
      OUT_BATCH((_3DSTATE_STENCIL_BUFFER << 16) | (3 - 2));
      OUT_BATCH(0);
      OUT_BATCH(0);
      ADVANCE_BATCH();
   }
}*/

/*
static void
gen6_blorp_emit_depth_disable(struct brw_context *brw,
                              const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(7);
   OUT_BATCH(_3DSTATE_DEPTH_BUFFER << 16 | (7 - 2));
   OUT_BATCH((BRW_DEPTHFORMAT_D32_FLOAT << 18) |
             (BRW_SURFACE_NULL << 29));
   OUT_BATCH(0);
   OUT_BATCH(0);
   OUT_BATCH(0);
   OUT_BATCH(0);
   OUT_BATCH(0);
   ADVANCE_BATCH();
}*/


/* 3DSTATE_CLEAR_PARAMS
 *
 * From the Sandybridge PRM, Volume 2, Part 1, Section 3DSTATE_CLEAR_PARAMS:
 *   [DevSNB] 3DSTATE_CLEAR_PARAMS packet must follow the DEPTH_BUFFER_STATE
 *   packet when HiZ is enabled and the DEPTH_BUFFER_STATE changes.
 */
/*
static void
gen6_blorp_emit_clear_params(struct brw_context *brw,
                             const brw_blorp_params *params)
{
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(2);
   OUT_BATCH(_3DSTATE_CLEAR_PARAMS << 16 |
	     GEN5_DEPTH_CLEAR_VALID |
	     (2 - 2));
   OUT_BATCH(params->depth.mt ? params->depth.mt->depth_clear_value : 0);
   ADVANCE_BATCH();
}*/


/* 3DPRIMITIVE */
static void
gen5_blorp_emit_primitive(struct brw_context *brw,
                          const brw_blorp_params *params)
{
   /* is this right for gen5 too? */
   struct intel_context *intel = &brw->intel;

   BEGIN_BATCH(6);
   OUT_BATCH(CMD_3D_PRIM << 16 | (6 - 2) |
             _3DPRIM_RECTLIST << GEN4_3DPRIM_TOPOLOGY_TYPE_SHIFT |
             GEN4_3DPRIM_VERTEXBUFFER_ACCESS_SEQUENTIAL);
   OUT_BATCH(3); /* vertex count per instance */
   OUT_BATCH(0);
   OUT_BATCH(1); /* instance count */
   OUT_BATCH(0);
   OUT_BATCH(0);
   ADVANCE_BATCH();
}


/**
 * \brief Execute a blit or render pass operation.
 *
 * To execute the operation, this function manually constructs and emits a
 * batch to draw a rectangle primitive. The batchbuffer is flushed before
 * constructing and after emitting the batch.
 *
 * This function alters no GL state.
 */
void
gen5_blorp_exec(struct intel_context *intel,
                const brw_blorp_params *params)
{
   struct gl_context *ctx = &intel->ctx;
   struct brw_context *brw = brw_context(ctx);
   brw_blorp_prog_data *prog_data = NULL;
   uint32_t wm_push_const_offset = 0;
   uint32_t wm_bind_bo_offset = 0;
   uint32_t vs_offset = 0;
   uint32_t clip_offset = 0;
   uint32_t sf_offset = 0;
   uint32_t wm_offset = 0;
   uint32_t cc_state_offset = 0;

   uint32_t prog_offset = params->get_wm_prog(brw, &prog_data);
   gen6_blorp_emit_batch_head(brw, params);

   gen5_blorp_emit_state_base_address(brw, params);
   gen5_blorp_emit_vertices(brw, params);
   gen5_blorp_emit_urb_config(brw, params);
   if (params->use_wm_prog) {
      cc_state_offset = gen5_blorp_emit_cc_unit_state(brw, params);
   }
   // DONE TO HERE ----
   if (params->use_wm_prog) {
      assert(!"not done");
      uint32_t wm_surf_offset_renderbuffer;
      uint32_t wm_surf_offset_texture = 0;
      uint32_t sampler_offset;
//      wm_push_const_offset = gen6_blorp_emit_wm_constants(brw, params);
      /* same as gen6 */
      wm_surf_offset_renderbuffer =
         gen6_blorp_emit_surface_state(brw, params, &params->dst,
                                       I915_GEM_DOMAIN_RENDER,
                                       I915_GEM_DOMAIN_RENDER);
      if (params->src.mt) {
         /* same as gen6 */
         wm_surf_offset_texture =
            gen6_blorp_emit_surface_state(brw, params, &params->src,
                                          I915_GEM_DOMAIN_SAMPLER, 0);
      }
      wm_bind_bo_offset =
         gen6_blorp_emit_binding_table(brw, params,
                                       wm_surf_offset_renderbuffer,
                                       wm_surf_offset_texture);
      sampler_offset = gen6_blorp_emit_sampler_state(brw, params);
      gen6_blorp_emit_sampler_state_pointers(brw, params, sampler_offset); */
   }

   vs_offset = gen5_blorp_emit_vs_disable(brw, params);
   clip_offset = gen5_blorp_emit_clip_disable(brw, params);
   sf_offset = gen5_blorp_emit_sf_config(brw, params);
   wm_offset = gen5_blorp_emit_wm_config(brw, params);

   gen5_blorp_emit_pipelined_pointers(brw, params, vs_offset,
         clip_offset, sf_offset, wm_offset, cc_state_offset);

/*
   if (params->use_wm_prog)
      gen6_blorp_emit_constant_ps(brw, params, wm_push_const_offset);
   else
      gen6_blorp_emit_constant_ps_disable(brw, params);
   gen6_blorp_emit_wm_config(brw, params, prog_offset, prog_data);
   if (params->use_wm_prog)
      gen6_blorp_emit_binding_table_pointers(brw, params, wm_bind_bo_offset);
   gen6_blorp_emit_viewport_state(brw, params);

   if (params->depth.mt)
      gen6_blorp_emit_depth_stencil_config(brw, params);
   else
      gen6_blorp_emit_depth_disable(brw, params);*/

   //gen6_blorp_emit_clear_params(brw, params);
   //
   gen6_blorp_emit_drawing_rectangle(brw, params);    /* same as Gen6 */

   gen5_blorp_emit_primitive(brw, params);
}

