/*
 Copyright (C) Intel Corp.  2006.  All Rights Reserved.
 Intel funded Tungsten Graphics (http://www.tungstengraphics.com) to
 develop this 3D driver.
 
 Permission is hereby granted, free of charge, to any person obtaining
 a copy of this software and associated documentation files (the
 "Software"), to deal in the Software without restriction, including
 without limitation the rights to use, copy, modify, merge, publish,
 distribute, sublicense, and/or sell copies of the Software, and to
 permit persons to whom the Software is furnished to do so, subject to
 the following conditions:
 
 The above copyright notice and this permission notice (including the
 next paragraph) shall be included in all copies or substantial
 portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 
 **********************************************************************/
 /*
  * Authors:
  *   Keith Whitwell <keith@tungstengraphics.com>
  */

#include "main/glheader.h"
#include "main/macros.h"
#include "main/enums.h"

#include "intel_batchbuffer.h"

#include "brw_defines.h"
#include "brw_context.h"
#include "brw_eu.h"
#include "brw_util.h"
#include "brw_state.h"
#include "brw_clip.h"

#include "glsl/ralloc.h"

#define FRONT_UNFILLED_BIT  0x1
#define BACK_UNFILLED_BIT   0x2


/* Set up interpolation modes for every element in the VUE */
static void
brw_setup_vue_interpolation(struct brw_context *brw)
{
   const struct gl_fragment_program *fprog = brw->fragment_program;
   /* XXX: is this the right vue_map? (GS!) */
   struct brw_vue_map *vue_map = &brw->vue_map_geom_out;

   /* XXX: stuffing this in the context is not really good enough */
   memset(brw->interpolation_mode, INTERP_QUALIFIER_NONE, BRW_VARYING_SLOT_COUNT);

   if (!fprog)
      return;

   for (int i=0; i < vue_map->num_slots; i++) {
      int varying = vue_map->slot_to_varying[i];
      if (varying == -1)
         continue;

      /* HPOS always wants noperspective. setting it up here allows
       * us to not need a w/a in the SF program. */
      if (varying == VARYING_SLOT_POS) {
         brw->interpolation_mode[i] = INTERP_QUALIFIER_NOPERSPECTIVE;
         continue;
      }

      int frag_attrib = varying;
      if (varying == VARYING_SLOT_BFC0 || varying == VARYING_SLOT_BFC1)
         frag_attrib = varying - VARYING_SLOT_BFC0 + VARYING_SLOT_COL0;

      if (!(fprog->Base.InputsRead & BITFIELD64_BIT(frag_attrib)))
         continue;

      enum glsl_interp_qualifier mode = fprog->InterpQualifier[frag_attrib];

      /* If the mode is not specified, the default varies: Color values
       * follow GL_SHADE_MODEL; everything else is smooth.
       */
      if (mode == INTERP_QUALIFIER_NONE) {
         if (frag_attrib == VARYING_SLOT_COL0 || frag_attrib == VARYING_SLOT_COL1)
            mode = brw->intel.ctx.Light.ShadeModel == GL_FLAT
               ? INTERP_QUALIFIER_FLAT : INTERP_QUALIFIER_SMOOTH;
         else
            mode = INTERP_QUALIFIER_SMOOTH;
      }

      brw->interpolation_mode[i] = mode;
   }
}


static void compile_clip_prog( struct brw_context *brw,
			     struct brw_clip_prog_key *key )
{
   struct intel_context *intel = &brw->intel;
   struct brw_clip_compile c;
   const GLuint *program;
   void *mem_ctx;
   GLuint program_size;
   GLuint i;

   memset(&c, 0, sizeof(c));

   mem_ctx = ralloc_context(NULL);
   
   /* Begin the compilation:
    */
   brw_init_compile(brw, &c.func, mem_ctx);

   c.func.single_program_flow = 1;

   c.key = *key;
   c.vue_map = brw->vue_map_geom_out;

   /* nr_regs is the number of registers filled by reading data from the VUE.
    * This program accesses the entire VUE, so nr_regs needs to be the size of
    * the VUE (measured in pairs, since two slots are stored in each
    * register).
    */
   c.nr_regs = (c.vue_map.num_slots + 1)/2;

   c.prog_data.clip_mode = c.key.clip_mode; /* XXX */

   /* For some reason the thread is spawned with only 4 channels
    * unmasked.  
    */
   brw_set_mask_control(&c.func, BRW_MASK_DISABLE);


   /* Would ideally have the option of producing a program which could
    * do all three:
    */
   switch (key->primitive) {
   case GL_TRIANGLES: 
      if (key->do_unfilled)
	 brw_emit_unfilled_clip( &c );
      else
	 brw_emit_tri_clip( &c );
      break;
   case GL_LINES:
      brw_emit_line_clip( &c );
      break;
   case GL_POINTS:
      brw_emit_point_clip( &c );
      break;
   default:
      assert(0);
      return;
   }

	 

   /* get the program
    */
   program = brw_get_program(&c.func, &program_size);

   if (unlikely(INTEL_DEBUG & DEBUG_CLIP)) {
      printf("clip:\n");
      for (i = 0; i < program_size / sizeof(struct brw_instruction); i++)
	 brw_disasm(stdout, &((struct brw_instruction *)program)[i],
		    intel->gen);
      printf("\n");
   }

   brw_upload_cache(&brw->cache,
		    BRW_CLIP_PROG,
		    &c.key, sizeof(c.key),
		    program, program_size,
		    &c.prog_data, sizeof(c.prog_data),
		    &brw->clip.prog_offset, &brw->clip.prog_data);
   ralloc_free(mem_ctx);
}

/* Calculate interpolants for triangle and line rasterization.
 */
static void
brw_upload_clip_prog(struct brw_context *brw)
{
   struct intel_context *intel = &brw->intel;
   struct gl_context *ctx = &intel->ctx;
   struct brw_clip_prog_key key;
   int i;

   memset(&key, 0, sizeof(key));

   /* Populate the key:
    */

   /* BRW_NEW_FRAGMENT_PROGRAM, _NEW_LIGHT */
   brw_setup_vue_interpolation(brw);
   memcpy(key.interpolation_mode, brw->interpolation_mode, BRW_VARYING_SLOT_COUNT);

   /* BRW_NEW_REDUCED_PRIMITIVE */
   key.primitive = brw->intel.reduced_primitive;
   /* BRW_NEW_VUE_MAP_GEOM_OUT */
   key.attrs = brw->vue_map_geom_out.slots_valid;

   /* BRW_NEW_FRAGMENT_PROGRAM, _NEW_LIGHT */
   key.has_flat_shading = 0;
   for (i = 0; i < brw->vue_map_geom_out.num_slots; i++) {
      if (key.interpolation_mode[i] == INTERP_QUALIFIER_FLAT) {
         key.has_flat_shading = 1;
         break;
      }
   }

   key.has_noperspective_shading = 0;
   for (i = 0; i < brw->vue_map_geom_out.num_slots; i++) {
      if (key.interpolation_mode[i] == INTERP_QUALIFIER_NOPERSPECTIVE) {
         key.has_noperspective_shading = 1;
         break;
      }
   }

   key.pv_first = (ctx->Light.ProvokingVertex == GL_FIRST_VERTEX_CONVENTION);
   /* _NEW_TRANSFORM (also part of VUE map)*/
   key.nr_userclip = _mesa_bitcount_64(ctx->Transform.ClipPlanesEnabled);

   if (intel->gen == 5)
       key.clip_mode = BRW_CLIPMODE_KERNEL_CLIP;
   else
       key.clip_mode = BRW_CLIPMODE_NORMAL;

   /* _NEW_POLYGON */
   if (key.primitive == GL_TRIANGLES) {
      if (ctx->Polygon.CullFlag &&
	  ctx->Polygon.CullFaceMode == GL_FRONT_AND_BACK)
	 key.clip_mode = BRW_CLIPMODE_REJECT_ALL;
      else {
	 GLuint fill_front = CLIP_CULL;
	 GLuint fill_back = CLIP_CULL;
	 GLuint offset_front = 0;
	 GLuint offset_back = 0;

	 if (!ctx->Polygon.CullFlag ||
	     ctx->Polygon.CullFaceMode != GL_FRONT) {
	    switch (ctx->Polygon.FrontMode) {
	    case GL_FILL: 
	       fill_front = CLIP_FILL; 
	       offset_front = 0;
	       break;
	    case GL_LINE:
	       fill_front = CLIP_LINE;
	       offset_front = ctx->Polygon.OffsetLine;
	       break;
	    case GL_POINT:
	       fill_front = CLIP_POINT;
	       offset_front = ctx->Polygon.OffsetPoint;
	       break;
	    }
	 }

	 if (!ctx->Polygon.CullFlag ||
	     ctx->Polygon.CullFaceMode != GL_BACK) {
	    switch (ctx->Polygon.BackMode) {
	    case GL_FILL: 
	       fill_back = CLIP_FILL; 
	       offset_back = 0;
	       break;
	    case GL_LINE:
	       fill_back = CLIP_LINE;
	       offset_back = ctx->Polygon.OffsetLine;
	       break;
	    case GL_POINT:
	       fill_back = CLIP_POINT;
	       offset_back = ctx->Polygon.OffsetPoint;
	       break;
	    }
	 }

	 if (ctx->Polygon.BackMode != GL_FILL ||
	     ctx->Polygon.FrontMode != GL_FILL) {
	    key.do_unfilled = 1;

	    /* Most cases the fixed function units will handle.  Cases where
	     * one or more polygon faces are unfilled will require help:
	     */
	    key.clip_mode = BRW_CLIPMODE_CLIP_NON_REJECTED;

	    if (offset_back || offset_front) {
	       /* _NEW_POLYGON, _NEW_BUFFERS */
	       key.offset_units = ctx->Polygon.OffsetUnits * ctx->DrawBuffer->_MRD * 2;
	       key.offset_factor = ctx->Polygon.OffsetFactor * ctx->DrawBuffer->_MRD;
	    }

	    switch (ctx->Polygon.FrontFace) {
	    case GL_CCW:
	       key.fill_ccw = fill_front;
	       key.fill_cw = fill_back;
	       key.offset_ccw = offset_front;
	       key.offset_cw = offset_back;
	       if (ctx->Light.Model.TwoSide &&
		   key.fill_cw != CLIP_CULL) 
		  key.copy_bfc_cw = 1;
	       break;
	    case GL_CW:
	       key.fill_cw = fill_front;
	       key.fill_ccw = fill_back;
	       key.offset_cw = offset_front;
	       key.offset_ccw = offset_back;
	       if (ctx->Light.Model.TwoSide &&
		   key.fill_ccw != CLIP_CULL) 
		  key.copy_bfc_ccw = 1;
	       break;
	    }
	 }
      }
   }

   if (!brw_search_cache(&brw->cache, BRW_CLIP_PROG,
			 &key, sizeof(key),
			 &brw->clip.prog_offset, &brw->clip.prog_data)) {
      compile_clip_prog( brw, &key );
   }
}


const struct brw_tracked_state brw_clip_prog = {
   .dirty = {
      .mesa  = (_NEW_LIGHT | 
		_NEW_TRANSFORM |
		_NEW_POLYGON | 
		_NEW_BUFFERS),
      .brw   = (BRW_NEW_FRAGMENT_PROGRAM |
                BRW_NEW_REDUCED_PRIMITIVE |
                BRW_NEW_VUE_MAP_GEOM_OUT)
   },
   .emit = brw_upload_clip_prog
};
