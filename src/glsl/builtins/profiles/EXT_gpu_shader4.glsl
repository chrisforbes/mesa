#extension GL_EXT_gpu_shader4: enable


/* 8.3 Common Functions */

int abs(int x);
ivec2  abs(ivec2  x);
ivec3  abs(ivec3  x);
ivec4  abs(ivec4  x);

int   sign(int   x);
ivec2 sign(ivec2 x);
ivec3 sign(ivec3 x);
ivec4 sign(ivec4 x);

int   min(int    x, int    y);
ivec2 min(ivec2  x, ivec2  y);
ivec3 min(ivec3  x, ivec3  y);
ivec4 min(ivec4  x, ivec4  y);
ivec2 min(ivec2  x, int  y);
ivec3 min(ivec3  x, int  y);
ivec4 min(ivec4  x, int  y);

unsigned int   min(unsigned int    x, unsigned int    y);
uvec2 min(uvec2  x, uvec2  y);
uvec3 min(uvec3  x, uvec3  y);
uvec4 min(uvec4  x, uvec4  y);
uvec2 min(uvec2  x, unsigned int  y);
uvec3 min(uvec3  x, unsigned int  y);
uvec4 min(uvec4  x, unsigned int  y);

int   max(int    x, int    y);
ivec2 max(ivec2  x, ivec2  y);
ivec3 max(ivec3  x, ivec3  y);
ivec4 max(ivec4  x, ivec4  y);
ivec2 max(ivec2  x, int  y);
ivec3 max(ivec3  x, int  y);
ivec4 max(ivec4  x, int  y);

unsigned int   max(unsigned int    x, unsigned int    y);
uvec2 max(uvec2  x, uvec2  y);
uvec3 max(uvec3  x, uvec3  y);
uvec4 max(uvec4  x, uvec4  y);
uvec2 max(uvec2  x, unsigned int  y);
uvec3 max(uvec3  x, unsigned int  y);
uvec4 max(uvec4  x, unsigned int  y);

int   clamp(int   x, int   minval, int    maxval);
ivec2   clamp(ivec2   x, ivec2   minval, ivec2    maxval);
ivec3   clamp(ivec3   x, ivec3   minval, ivec3    maxval);
ivec4   clamp(ivec4   x, ivec4   minval, ivec4    maxval);
int   clamp(int   x, int   minval, int    maxval);
ivec2   clamp(ivec2   x, int   minval, int    maxval);
ivec3   clamp(ivec3   x, int   minval, int    maxval);
ivec4   clamp(ivec4   x, int   minval, int    maxval);

unsigned int   clamp(unsigned int   x, unsigned int   minval, unsigned int    maxval);
uvec2   clamp(uvec2   x, uvec2   minval, uvec2    maxval);
uvec3   clamp(uvec3   x, uvec3   minval, uvec3    maxval);
uvec4   clamp(uvec4   x, uvec4   minval, uvec4    maxval);
unsigned int   clamp(unsigned int   x, unsigned int   minval, unsigned int    maxval);
uvec2   clamp(uvec2   x, unsigned int   minval, unsigned int    maxval);
uvec3   clamp(uvec3   x, unsigned int   minval, unsigned int    maxval);
uvec4   clamp(uvec4   x, unsigned int   minval, unsigned int    maxval);

float truncate(float x);
vec2  truncate(vec2  x);
vec3  truncate(vec3  x);
vec4  truncate(vec4  x);

float round(float x);
vec2  round(vec2  x);
vec3  round(vec3  x);
vec4  round(vec4  x);


/* 8.6 Vector relational functions */

bvec2 lessThan(uvec2 x, uvec2 y);
bvec3 lessThan(uvec3 x, uvec3 y);
bvec4 lessThan(uvec4 x, uvec4 y);

bvec2 lessThanEqual(uvec2 x, uvec2 y);
bvec3 lessThanEqual(uvec3 x, uvec3 y);
bvec4 lessThanEqual(uvec4 x, uvec4 y);

bvec2 greaterThan(uvec2 x, uvec2 y);
bvec3 greaterThan(uvec3 x, uvec3 y);
bvec4 greaterThan(uvec4 x, uvec4 y);

bvec2 greaterThanEqual(uvec2 x, uvec2 y);
bvec3 greaterThanEqual(uvec3 x, uvec3 y);
bvec4 greaterThanEqual(uvec4 x, uvec4 y);

bvec2 equal(uvec2 x, uvec2 y);
bvec3 equal(uvec3 x, uvec3 y);
bvec4 equal(uvec4 x, uvec4 y);

bvec2 notEqual(uvec2 x, uvec2 y);
bvec3 notEqual(uvec3 x, uvec3 y);
bvec4 notEqual(uvec4 x, uvec4 y);


/* 8.7 Texture lookup functions */

ivec4 texture1D(isampler1D sampler, float coord);
uvec4 texture1D(usampler1D sampler, float coord);
ivec4 texture2D(isampler2D sampler, vec2 coord);
uvec4 texture2D(usampler2D sampler, vec2 coord);
ivec4 texture3D(isampler2D sampler, vec3 coord);
uvec4 texture3D(usampler3D sampler, vec3 coord);

ivec4 texture1DProj(isampler1D sampler, vec2 coord);
uvec4 texture1DProj(usampler1D sampler, vec2 coord);
ivec4 texture2DProj(isampler2D sampler, vec3 coord);
uvec4 texture2DProj(usampler2D sampler, vec3 coord);
ivec4 texture3DProj(isampler2D sampler, vec4 coord);
uvec4 texture3DProj(usampler3D sampler, vec4 coord);
ivec4 texture1DProj(isampler1D sampler, vec4 coord);
uvec4 texture1DProj(usampler1D sampler, vec4 coord);
ivec4 texture2DProj(isampler2D sampler, vec4 coord);
uvec4 texture2DProj(usampler2D sampler, vec4 coord);

ivec4 textureCube(isamplerCube sampler, vec3 coord);
uvec4 textureCube(usamplerCube sampler, vec3 coord);


vec4 texelFetch1D(sampler1D sampler, int coord, int lod);
vec4 texelFetch2D(sampler2D sampler, ivec2 coord, int lod);
vec4 texelFetch3D(sampler3D sampler, ivec3 coord, int lod);
vec4 texelFetch2DRect(sampler2DRect sampler, ivec2 coord);
vec4 texelFetch1DArray(sampler1DArray sampler, ivec2 coord, int lod);
vec4 texelFetch2DArray(sampler2DArray sampler, ivec3 coord, int lod);

ivec4 texelFetch1D(isampler1D sampler, int coord, int lod);
ivec4 texelFetch2D(isampler2D sampler, ivec2 coord, int lod);
ivec4 texelFetch3D(isampler3D sampler, ivec3 coord, int lod);
ivec4 texelFetch2DRect(isampler2DRect sampler, ivec2 coord);
ivec4 texelFetch1DArray(isampler1DArray sampler, ivec2 coord, int lod);
ivec4 texelFetch2DArray(isampler2DArray sampler, ivec3 coord, int lod);

uvec4 texelFetch1D(usampler1D sampler, int coord, int lod);
uvec4 texelFetch2D(usampler2D sampler, ivec2 coord, int lod);
uvec4 texelFetch3D(usampler3D sampler, ivec3 coord, int lod);
uvec4 texelFetch2DRect(usampler2DRect sampler, ivec2 coord);
uvec4 texelFetch1DArray(usampler1DArray sampler, ivec2 coord, int lod);
uvec4 texelFetch2DArray(usampler2DArray sampler, ivec3 coord, int lod);


vec4 texelFetchBuffer(samplerBuffer sampler, int coord);
ivec4 texelFetchBuffer(isamplerBuffer sampler, int coord);
uvec4 texelFetchBuffer(usamplerBuffer sampler, int coord);


int textureSizeBuffer(samplerBuffer sampler);
int textureSize1D(sampler1D sampler, int lod);
ivec2 textureSize2D(sampler2D sampler, int lod);
ivec3 textureSize3D(sampler3D sampler, int lod);
ivec2 textureSizeCube(samplerCube sampler, int lod);
ivec2 textureSize2DRect(sampler2DRect sampler, int lod);
ivec2 textureSize1DArray(sampler1DArray sampler, int lod);
ivec3 textureSize2DArray(sampler2DArray sampler, int lod);

int textureSizeBuffer(isamplerBuffer sampler);
int textureSize1D(isampler1D sampler, int lod);
ivec2 textureSize2D(isampler2D sampler, int lod);
ivec3 textureSize3D(isampler3D sampler, int lod);
ivec2 textureSizeCube(isamplerCube sampler, int lod);
ivec2 textureSize2DRect(isampler2DRect sampler, int lod);
ivec2 textureSize1DArray(isampler1DArray sampler, int lod);
ivec3 textureSize2DArray(isampler2DArray sampler, int lod);

int textureSizeBuffer(usamplerBuffer sampler);
int textureSize1D(usampler1D sampler, int lod);
ivec2 textureSize2D(usampler2D sampler, int lod);
ivec3 textureSize3D(usampler3D sampler, int lod);
ivec2 textureSizeCube(usamplerCube sampler, int lod);
ivec2 textureSize2DRect(usampler2DRect sampler, int lod);
ivec2 textureSize1DArray(usampler1DArray sampler, int lod);
ivec3 textureSize2DArray(usampler2DArray sampler, int lod);


vec4 texture1DArray(sampler1DArray sampler, vec2 coord);
ivec4 texture1DArray(isampler1DArray sampler, vec2 coord);
uvec4 texture1DArray(usampler1DArray sampler, vec2 coord);
vec4 texture1DArrayLod(sampler1DArray sampler, vec2 coord, float lod);
ivec4 texture1DArrayLod(isampler1DArray sampler, vec2 coord, float lod);
uvec4 texture1DArrayLod(usampler1DArray sampler, vec2 coord, float lod);


vec4 texture2DArray(sampler2DArray sampler, vec3 coord);
ivec4 texture2DArray(isampler2DArray sampler, vec3 coord);
uvec4 texture2DArray(usampler2DArray sampler, vec3 coord);
vec4 texture2DArrayLod(sampler2DArray sampler, vec3 coord, float lod);
ivec4 texture2DArrayLod(isampler2DArray sampler, vec3 coord, float lod);
uvec4 texture2DArrayLod(usampler2DArray sampler, vec3 coord, float lod);


vec4 shadow1DArray(sampler1DArrayShadow sampler, vec3 coord);
vec4 shadow1DArrayLod(sampler1DArrayShadow sampler, vec3 coord, float lod);
vec4 shadow2DArray(sampler2DArrayShadow sampler, vec4 coord);


vec4 shadowCube(samplerCubeShadow sampler, vec4 coord);


vec4 texture1DGrad(sampler1D sampler, float coord, float ddx, float ddy);
vec4 texture1DProjGrad(sampler1D sampler, vec2 coord, float ddx, float ddy);
vec4 texture1DProjGrad(sampler1D sampler, vec4 coord, float ddx, float ddy);
vec4 texture1DArrayGrad(sampler1DArray sampler, vec2 coord, float ddx, float ddy);

vec4 texture2DGrad(sampler2D sampler, vec2 coord, vec2 ddx, vec2 ddy);
vec4 texture2DProjGrad(sampler2D sampler, vec3 coord, vec2 ddx, vec2 ddy);
vec4 texture2DProjGrad(sampler2D sampler, vec4 coord, vec2 ddx, vec2 ddy);
vec4 texture2DArrayGrad(sampler2DArray sampler, vec3 coord, vec2 ddx, vec2 ddy);

vec4 texture3DGrad(sampler3D sampler, vec3 coord, vec3 ddx, vec3 ddy);
vec4 texture3DProjGrad(sampler3D sampler, vec4 coord, vec3 ddx, vec3 ddy);

vec4 textureCubeGrad(samplerCube sampler, vec3 coord, vec3 ddx, vec3 ddy);

ivec4 texture1DGrad(isampler1D sampler, float coord, float ddx, float ddy);
ivec4 texture1DProjGrad(isampler1D sampler, vec2 coord, float ddx, float ddy);
ivec4 texture1DProjGrad(isampler1D sampler, vec4 coord, float ddx, float ddy);
ivec4 texture1DArrayGrad(isampler1DArray sampler, vec2 coord, float ddx, float ddy);

ivec4 texture2DGrad(isampler2D sampler, vec2 coord, vec2 ddx, vec2 ddy);
ivec4 texture2DProjGrad(isampler2D sampler, vec3 coord, vec2 ddx, vec2 ddy);
ivec4 texture2DProjGrad(isampler2D sampler, vec4 coord, vec2 ddx, vec2 ddy);
ivec4 texture2DArrayGrad(isampler2DArray sampler, vec3 coord, vec2 ddx, vec2 ddy);

ivec4 texture3DGrad(isampler3D sampler, vec3 coord, vec3 ddx, vec3 ddy);
ivec4 texture3DProjGrad(isampler3D sampler, vec4 coord, vec3 ddx, vec3 ddy);

ivec4 textureCubeGrad(isamplerCube sampler, vec3 coord, vec3 ddx, vec3 ddy);

uvec4 texture1DGrad(usampler1D sampler, float coord, float ddx, float ddy);
uvec4 texture1DProjGrad(usampler1D sampler, vec2 coord, float ddx, float ddy);
uvec4 texture1DProjGrad(usampler1D sampler, vec4 coord, float ddx, float ddy);
uvec4 texture1DArrayGrad(usampler1DArray sampler, vec2 coord, float ddx, float ddy);

uvec4 texture2DGrad(usampler2D sampler, vec2 coord, vec2 ddx, vec2 ddy);
uvec4 texture2DProjGrad(usampler2D sampler, vec3 coord, vec2 ddx, vec2 ddy);
uvec4 texture2DProjGrad(usampler2D sampler, vec4 coord, vec2 ddx, vec2 ddy);
uvec4 texture2DArrayGrad(usampler2DArray sampler, vec3 coord, vec2 ddx, vec2 ddy);

uvec4 texture3DGrad(usampler3D sampler, vec3 coord, vec3 ddx, vec3 ddy);
uvec4 texture3DProjGrad(usampler3D sampler, vec4 coord, vec3 ddx, vec3 ddy);

uvec4 textureCubeGrad(usamplerCube sampler, vec3 coord, vec3 ddx, vec3 ddy);


vec4 shadow1DGrad(sampler1DShadow sampler, vec3 coord, float ddx, float ddy);
vec4 shadow1DProjGrad(sampler1DShadow sampler, vec4 coord, float ddx, float ddy);
vec4 shadow1DArrayGrad(sampler1DArrayShadow sampler, vec3 coord, float ddx, float ddy);

vec4 shadow2DGrad(sampler2DShadow sampler, vec3 coord, vec2 ddx, vec2 ddy);
vec4 shadow2DProjGrad(sampler2DShadow sampler, vec4 coord, vec2 ddx, vec2 ddy);
vec4 shadow2DArrayGrad(sampler2DArrayShadow sampler, vec4 coord, vec2 ddx, vec2 ddy);

vec4 texture2DRectGrad(sampler2DRect sampler, vec2 coord, vec2 ddx, vec2 ddy);
vec4 texture2DRectProjGrad(sampler2DRect sampler, vec3 coord, vec2 ddx, vec2 ddy);
vec4 texture2DRectProjGrad(sampler2DRect sampler, vec4 coord, vec2 ddx, vec2 ddy);

ivec4 texture2DRectGrad(isampler2DRect sampler, vec2 coord, vec2 ddx, vec2 ddy);
ivec4 texture2DRectProjGrad(isampler2DRect sampler, vec3 coord, vec2 ddx, vec2 ddy);
ivec4 texture2DRectProjGrad(isampler2DRect sampler, vec4 coord, vec2 ddx, vec2 ddy);

uvec4 texture2DRectGrad(usampler2DRect sampler, vec2 coord, vec2 ddx, vec2 ddy);
uvec4 texture2DRectProjGrad(usampler2DRect sampler, vec3 coord, vec2 ddx, vec2 ddy);
uvec4 texture2DRectProjGrad(usampler2DRect sampler, vec4 coord, vec2 ddx, vec2 ddy);

vec4 shadow2DRectGrad(sampler2DRectShadow sampler, vec3 coord, vec2 ddx, vec2 ddy);
vec4 shadow2DRectProjGrad(sampler2DRectShadow sampler, vec4 coord, vec2 ddx, vec2 ddy);

vec4 shadowCubeGrad(samplerCubeShadow sampler, vec4 coord, vec3 ddx, vec3 ddy);


vec4 texture1DOffset(sampler1D sampler, float coord, int offset );
vec4 texture1DProjOffset(sampler1D sampler, vec2 coord, int offset );
vec4 texture1DProjOffset(sampler1D sampler, vec4 coord, int offset );
vec4 texture1DLodOffset(sampler1D sampler, float coord, float lod, int offset);
vec4 texture1DProjLodOffset(sampler1D sampler, vec2 coord, float lod, int offset);
vec4 texture1DProjLodOffset(sampler1D sampler, vec4 coord, float lod, int offset);

vec4 texture2DOffset(sampler2D sampler, vec2 coord, ivec2 offset );
vec4 texture2DProjOffset(sampler2D sampler, vec3 coord, ivec2 offset );
vec4 texture2DProjOffset(sampler2D sampler, vec4 coord, ivec2 offset );
vec4 texture2DLodOffset(sampler2D sampler, vec2 coord, float lod, ivec2 offset);
vec4 texture2DProjLodOffset(sampler2D sampler, vec3 coord, float lod, ivec2 offset);
vec4 texture2DProjLodOffset(sampler2D sampler, vec4 coord, float lod, ivec2 offset);

vec4 texture3DOffset(sampler3D sampler, vec3 coord, ivec3 offset );
vec4 texture3DProjOffset(sampler3D sampler, vec4 coord, ivec3 offset );
vec4 texture3DLodOffset(sampler3D sampler, vec3 coord, float lod, ivec3 offset);
vec4 texture3DProjLodOffset(sampler3D sampler, vec4 coord, float lod, ivec3 offset);

vec4 shadow1DOffset(sampler1DShadow sampler, vec3 coord, int offset );
vec4 shadow2DOffset(sampler2DShadow sampler, vec3 coord, ivec2 offset );
vec4 shadow1DProjOffset(sampler1DShadow sampler, vec4 coord, int offset );
vec4 shadow2DProjOffset(sampler2DShadow sampler, vec4 coord, ivec2 offset );
vec4 shadow1DLodOffset(sampler1DShadow sampler, vec3 coord, float lod, int offset);
vec4 shadow2DLodOffset(sampler2DShadow sampler, vec3 coord, float lod, ivec2 offset);
vec4 shadow1DProjLodOffset(sampler1DShadow sampler, vec4 coord, float lod, int offset);
vec4 shadow2DProjLodOffset(sampler2DShadow sampler, vec4 coord, float lod, ivec2 offset);

vec4 texture2DRectOffset(sampler2DRect sampler, vec2 coord, ivec2 offset);
vec4 texture2DRectProjOffset(sampler2DRect sampler, vec3 coord, ivec2 offset);
vec4 texture2DRectProjOffset(sampler2DRect sampler, vec4 coord, ivec2 offset);
vec4 shadow2DRectOffset(sampler2DRectShadow sampler, vec3 coord, ivec2 offset);
vec4 shadow2DRectProjOffset(sampler2DRectShadow sampler, vec4 coord, ivec2 offset);

vec4 texelFetch1DOffset(sampler1D sampler, int coord, int lod, int offset);
vec4 texelFetch2DOffset(sampler2D sampler, ivec2 coord, int lod, ivec2 offset);
vec4 texelFetch3DOffset(sampler3D sampler, ivec3 coord, int lod, ivec3 offset);
vec4 texelFetch2DRectOffset(sampler2DRect sampler, ivec2 coord, ivec2 offset);
vec4 texelFetch1DArrayOffset(sampler1DArray sampler, ivec2 coord, int lod, int offset);
vec4 texelFetch2DArrayOffset(sampler2DArray sampler, ivec3 coord, int lod, ivec2 offset);

vec4 texture1DArrayOffset(sampler1DArray sampler, vec2 coord, int offset );
vec4 texture1DArrayLodOffset(sampler1DArray sampler, vec2 coord, float lod, int offset);

vec4 texture2DArrayOffset(sampler2DArray sampler, vec3 coord, ivec2 offset );
vec4 texture2DArrayLodOffset(sampler2DArray sampler, vec3 coord, float lod, ivec2 offset);

vec4 shadow1DArrayOffset(sampler1DArrayShadow sampler, vec3 coord, int offset );
vec4 shadow1DArrayLodOffset(sampler1DArrayShadow sampler, vec3 coord, float lod, int offset);

vec4 shadow2DArrayOffset(sampler2DArrayShadow sampler, vec4 coord, ivec2 offset);

vec4 texture1DGradOffset(sampler1D sampler, float coord, float ddx, float ddy, int offset);
vec4 texture1DProjGradOffset(sampler1D sampler, vec2 coord, float ddx, float ddy, int offset);
vec4 texture1DProjGradOffset(sampler1D sampler, vec4 coord, float ddx, float ddy, int offset);
vec4 texture1DArrayGradOffset(sampler1DArray sampler, vec2 coord, float ddx, float ddy, int offset);

vec4 texture2DGradOffset(sampler2D sampler, vec2 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 texture2DProjGradOffset(sampler2D sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 texture2DProjGradOffset(sampler2D sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 texture2DArrayGradOffset(sampler2DArray sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);

vec4 texture3DGradOffset(sampler3D sampler, vec3 coord, vec3 ddx, vec3 ddy, ivec3 offset);
vec4 texture3DProjGradOffset(sampler3D sampler, vec4 coord, vec3 ddx, vec3 ddy, ivec3 offset);

vec4 shadow1DGradOffset(sampler1DShadow sampler, vec3 coord, float ddx, float ddy, int offset);
vec4 shadow1DProjGradOffset(sampler1DShadow sampler, vec4 coord, float ddx, float ddy, int offset);
vec4 shadow1DArrayGradOffset(sampler1DArrayShadow sampler, vec3 coord, float ddx, float ddy, int offset);

vec4 shadow2DGradOffset(sampler2DShadow sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 shadow2DProjGradOffset(sampler2DShadow sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 shadow2DArrayGradOffset(sampler2DArrayShadow sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);

vec4 texture2DRectGradOffset(sampler2DRect sampler, vec2 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 texture2DRectProjGradOffset(sampler2DRect sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 texture2DRectProjGradOffset(sampler2DRect sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);

vec4 shadow2DRectGradOffset(sampler2DRectShadow sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
vec4 shadow2DRectProjGradOffset(sampler2DRectShadow sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);


ivec4 texture1DOffset(isampler1D sampler, float coord, int offset );
ivec4 texture1DProjOffset(isampler1D sampler, vec2 coord, int offset );
ivec4 texture1DProjOffset(isampler1D sampler, vec4 coord, int offset );
ivec4 texture1DLodOffset(isampler1D sampler, float coord, float lod, int offset);
ivec4 texture1DProjLodOffset(isampler1D sampler, vec2 coord, float lod, int offset);
ivec4 texture1DProjLodOffset(isampler1D sampler, vec4 coord, float lod, int offset);

ivec4 texture2DOffset(isampler2D sampler, vec2 coord, ivec2 offset );
ivec4 texture2DProjOffset(isampler2D sampler, vec3 coord, ivec2 offset );
ivec4 texture2DProjOffset(isampler2D sampler, vec4 coord, ivec2 offset );
ivec4 texture2DLodOffset(isampler2D sampler, vec2 coord, float lod, ivec2 offset);
ivec4 texture2DProjLodOffset(isampler2D sampler, vec3 coord, float lod, ivec2 offset);
ivec4 texture2DProjLodOffset(isampler2D sampler, vec4 coord, float lod, ivec2 offset);

ivec4 texture3DOffset(isampler3D sampler, vec3 coord, ivec3 offset );
ivec4 texture3DProjOffset(isampler3D sampler, vec4 coord, ivec3 offset );
ivec4 texture3DLodOffset(isampler3D sampler, vec3 coord, float lod, ivec3 offset);
ivec4 texture3DProjLodOffset(isampler3D sampler, vec4 coord, float lod, ivec3 offset);

ivec4 texture2DRectOffset(isampler2DRect sampler, vec2 coord, ivec2 offset);
ivec4 texture2DRectProjOffset(isampler2DRect sampler, vec3 coord, ivec2 offset);
ivec4 texture2DRectProjOffset(isampler2DRect sampler, vec4 coord, ivec2 offset);

ivec4 texelFetch1DOffset(isampler1D sampler, int coord, int lod, int offset);
ivec4 texelFetch2DOffset(isampler2D sampler, ivec2 coord, int lod, ivec2 offset);
ivec4 texelFetch3DOffset(isampler3D sampler, ivec3 coord, int lod, ivec3 offset);
ivec4 texelFetch2DRectOffset(isampler2DRect sampler, ivec2 coord, ivec2 offset);
ivec4 texelFetch1DArrayOffset(isampler1DArray sampler, ivec2 coord, int lod, int offset);
ivec4 texelFetch2DArrayOffset(isampler2DArray sampler, ivec3 coord, int lod, ivec2 offset);

ivec4 texture1DArrayOffset(isampler1DArray sampler, vec2 coord, int offset );
ivec4 texture1DArrayLodOffset(isampler1DArray sampler, vec2 coord, float lod, int offset);

ivec4 texture2DArrayOffset(isampler2DArray sampler, vec3 coord, ivec2 offset );
ivec4 texture2DArrayLodOffset(isampler2DArray sampler, vec3 coord, float lod, ivec2 offset);

ivec4 texture1DGradOffset(isampler1D sampler, float coord, float ddx, float ddy, int offset);
ivec4 texture1DProjGradOffset(isampler1D sampler, vec2 coord, float ddx, float ddy, int offset);
ivec4 texture1DProjGradOffset(isampler1D sampler, vec4 coord, float ddx, float ddy, int offset);
ivec4 texture1DArrayGradOffset(isampler1DArray sampler, vec2 coord, float ddx, float ddy, int offset);

ivec4 texture2DGradOffset(isampler2D sampler, vec2 coord, vec2 ddx, vec2 ddy, ivec2 offset);
ivec4 texture2DProjGradOffset(isampler2D sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
ivec4 texture2DProjGradOffset(isampler2D sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);
ivec4 texture2DArrayGradOffset(isampler2DArray sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);

ivec4 texture3DGradOffset(isampler3D sampler, vec3 coord, vec3 ddx, vec3 ddy, ivec3 offset);
ivec4 texture3DProjGradOffset(isampler3D sampler, vec4 coord, vec3 ddx, vec3 ddy, ivec3 offset);

ivec4 texture2DRectGradOffset(isampler2DRect sampler, vec2 coord, vec2 ddx, vec2 ddy, ivec2 offset);
ivec4 texture2DRectProjGradOffset(isampler2DRect sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
ivec4 texture2DRectProjGradOffset(isampler2DRect sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);


uvec4 texture1DOffset(usampler1D sampler, float coord, int offset );
uvec4 texture1DProjOffset(usampler1D sampler, vec2 coord, int offset );
uvec4 texture1DProjOffset(usampler1D sampler, vec4 coord, int offset );
uvec4 texture1DLodOffset(usampler1D sampler, float coord, float lod, int offset);
uvec4 texture1DProjLodOffset(usampler1D sampler, vec2 coord, float lod, int offset);
uvec4 texture1DProjLodOffset(usampler1D sampler, vec4 coord, float lod, int offset);

uvec4 texture2DOffset(usampler2D sampler, vec2 coord, ivec2 offset );
uvec4 texture2DProjOffset(usampler2D sampler, vec3 coord, ivec2 offset );
uvec4 texture2DProjOffset(usampler2D sampler, vec4 coord, ivec2 offset );
uvec4 texture2DLodOffset(usampler2D sampler, vec2 coord, float lod, ivec2 offset);
uvec4 texture2DProjLodOffset(usampler2D sampler, vec3 coord, float lod, ivec2 offset);
uvec4 texture2DProjLodOffset(usampler2D sampler, vec4 coord, float lod, ivec2 offset);

uvec4 texture3DOffset(usampler3D sampler, vec3 coord, ivec3 offset );
uvec4 texture3DProjOffset(usampler3D sampler, vec4 coord, ivec3 offset );
uvec4 texture3DLodOffset(usampler3D sampler, vec3 coord, float lod, ivec3 offset);
uvec4 texture3DProjLodOffset(usampler3D sampler, vec4 coord, float lod, ivec3 offset);

uvec4 texture2DRectOffset(usampler2DRect sampler, vec2 coord, ivec2 offset);
uvec4 texture2DRectProjOffset(usampler2DRect sampler, vec3 coord, ivec2 offset);
uvec4 texture2DRectProjOffset(usampler2DRect sampler, vec4 coord, ivec2 offset);

uvec4 texelFetch1DOffset(usampler1D sampler, int coord, int lod, int offset);
uvec4 texelFetch2DOffset(usampler2D sampler, ivec2 coord, int lod, ivec2 offset);
uvec4 texelFetch3DOffset(usampler3D sampler, ivec3 coord, int lod, ivec3 offset);
uvec4 texelFetch2DRectOffset(usampler2DRect sampler, ivec2 coord, ivec2 offset);
uvec4 texelFetch1DArrayOffset(usampler1DArray sampler, ivec2 coord, int lod, int offset);
uvec4 texelFetch2DArrayOffset(usampler2DArray sampler, ivec3 coord, int lod, ivec2 offset);

uvec4 texture1DArrayOffset(usampler1DArray sampler, vec2 coord, int offset );
uvec4 texture1DArrayLodOffset(usampler1DArray sampler, vec2 coord, float lod, int offset);

uvec4 texture2DArrayOffset(usampler2DArray sampler, vec3 coord, ivec2 offset );
uvec4 texture2DArrayLodOffset(usampler2DArray sampler, vec3 coord, float lod, ivec2 offset);

uvec4 texture1DGradOffset(usampler1D sampler, float coord, float ddx, float ddy, int offset);
uvec4 texture1DProjGradOffset(usampler1D sampler, vec2 coord, float ddx, float ddy, int offset);
uvec4 texture1DProjGradOffset(usampler1D sampler, vec4 coord, float ddx, float ddy, int offset);
uvec4 texture1DArrayGradOffset(usampler1DArray sampler, vec2 coord, float ddx, float ddy, int offset);

uvec4 texture2DGradOffset(usampler2D sampler, vec2 coord, vec2 ddx, vec2 ddy, ivec2 offset);
uvec4 texture2DProjGradOffset(usampler2D sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
uvec4 texture2DProjGradOffset(usampler2D sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);
uvec4 texture2DArrayGradOffset(usampler2DArray sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);

uvec4 texture3DGradOffset(usampler3D sampler, vec3 coord, vec3 ddx, vec3 ddy, ivec3 offset);
uvec4 texture3DProjGradOffset(usampler3D sampler, vec4 coord, vec3 ddx, vec3 ddy, ivec3 offset);

uvec4 texture2DRectGradOffset(usampler2DRect sampler, vec2 coord, vec2 ddx, vec2 ddy, ivec2 offset);
uvec4 texture2DRectProjGradOffset(usampler2DRect sampler, vec3 coord, vec2 ddx, vec2 ddy, ivec2 offset);
uvec4 texture2DRectProjGradOffset(usampler2DRect sampler, vec4 coord, vec2 ddx, vec2 ddy, ivec2 offset);
