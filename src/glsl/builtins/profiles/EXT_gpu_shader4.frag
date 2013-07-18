#extension GL_EXT_gpu_shader4: enable

/* 8.7 Texture lookup functions */

/* bias variants */
ivec4 texture1D(isampler1D sampler, float coord, float bias);
uvec4 texture1D(usampler1D sampler, float coord, float bias);

ivec4 texture2D(isampler2D sampler, vec2 coord, float bias);
uvec4 texture2D(usampler2D sampler, vec2 coord, float bias);

ivec4 texture3D(isampler2D sampler, vec3 coord, float bias);
uvec4 texture3D(usampler3D sampler, vec3 coord, float bias);

ivec4 texture1DProj(isampler1D sampler, vec2 coord, float bias);
uvec4 texture1DProj(usampler1D sampler, vec2 coord, float bias);

ivec4 texture2DProj(isampler2D sampler, vec3 coord, float bias);
uvec4 texture2DProj(usampler2D sampler, vec3 coord, float bias);

ivec4 texture3DProj(isampler2D sampler, vec4 coord, float bias);
uvec4 texture3DProj(usampler3D sampler, vec4 coord, float bias);

ivec4 texture1DProj(isampler1D sampler, vec4 coord, float bias);
uvec4 texture1DProj(usampler1D sampler, vec4 coord, float bias);

ivec4 texture2DProj(isampler2D sampler, vec4 coord, float bias);
uvec4 texture2DProj(usampler2D sampler, vec4 coord, float bias);

ivec4 textureCube(isamplerCube sampler, vec3 coord, float bias);
uvec4 textureCube(usamplerCube sampler, vec3 coord, float bias);


vec4 texture1DArray(sampler1DArray sampler, vec2 coord, float bias);
ivec4 texture1DArray(isampler1DArray sampler, vec2 coord, float bias);
uvec4 texture1DArray(usampler1DArray sampler, vec2 coord, float bias);

vec4 texture2DArray(sampler2DArray sampler, vec3 coord, float bias);
ivec4 texture2DArray(isampler2DArray sampler, vec3 coord, float bias);
uvec4 texture2DArray(usampler2DArray sampler, vec3 coord, float bias);

vec4 shadow1DArray(sampler1DArrayShadow sampler, vec3 coord, float bias);


vec4 texture1DOffset(sampler1D sampler, float coord, int offset, float bias);
vec4 texture1DProjOffset(sampler1D sampler, vec2 coord, int offset, float bias);
vec4 texture1DProjOffset(sampler1D sampler, vec4 coord, int offset, float bias);

vec4 texture2DOffset(sampler2D sampler, vec2 coord, ivec2 offset, float bias);
vec4 texture2DProjOffset(sampler2D sampler, vec3 coord, ivec2 offset, float bias);
vec4 texture2DProjOffset(sampler2D sampler, vec4 coord, ivec2 offset, float bias);

vec4 texture3DOffset(sampler3D sampler, vec3 coord, ivec3 offset, float bias);
vec4 texture3DProjOffset(sampler3D sampler, vec4 coord, ivec3 offset, float bias);

vec4 shadow1DOffset(sampler1DShadow sampler, vec3 coord, int offset, float bias);
vec4 shadow2DOffset(sampler2DShadow sampler, vec3 coord, ivec2 offset, float bias);
vec4 shadow1DProjOffset(sampler1DShadow sampler, vec4 coord, int offset, float bias);
vec4 shadow2DProjOffset(sampler2DShadow sampler, vec4 coord, ivec2 offset, float bias);

vec4 texture1DArrayOffset(sampler1DArray sampler, vec2 coord, int offset,  float bias);

vec4 texture2DArrayOffset(sampler2DArray sampler, vec3 coord, ivec2 offset,  float bias);

vec4 shadow1DArrayOffset(sampler1DArrayShadow sampler, vec3 coord, int offset, float bias);


ivec4 texture1DOffset(isampler1D sampler, float coord, int offset, float bias);
ivec4 texture1DProjOffset(isampler1D sampler, vec2 coord, int offset, float bias);
ivec4 texture1DProjOffset(isampler1D sampler, vec4 coord, int offset, float bias);

ivec4 texture2DOffset(isampler2D sampler, vec2 coord, ivec2 offset, float bias);
ivec4 texture2DProjOffset(isampler2D sampler, vec3 coord, ivec2 offset, float bias);
ivec4 texture2DProjOffset(isampler2D sampler, vec4 coord, ivec2 offset, float bias);

ivec4 texture3DOffset(isampler3D sampler, vec3 coord, ivec3 offset, float bias);
ivec4 texture3DProjOffset(isampler3D sampler, vec4 coord, ivec3 offset, float bias);

ivec4 texture1DArrayOffset(isampler1DArray sampler, vec2 coord, int offset,  float bias);

ivec4 texture2DArrayOffset(isampler2DArray sampler, vec3 coord, ivec2 offset,  float bias);


uvec4 texture1DOffset(usampler1D sampler, float coord, int offset, float bias);
uvec4 texture1DProjOffset(usampler1D sampler, vec2 coord, int offset, float bias);
uvec4 texture1DProjOffset(usampler1D sampler, vec4 coord, int offset, float bias);

uvec4 texture2DOffset(usampler2D sampler, vec2 coord, ivec2 offset, float bias);
uvec4 texture2DProjOffset(usampler2D sampler, vec3 coord, ivec2 offset, float bias);
uvec4 texture2DProjOffset(usampler2D sampler, vec4 coord, ivec2 offset, float bias);

uvec4 texture3DOffset(usampler3D sampler, vec3 coord, ivec3 offset, float bias);
uvec4 texture3DProjOffset(usampler3D sampler, vec4 coord, ivec3 offset, float bias);

uvec4 texture1DArrayOffset(usampler1DArray sampler, vec2 coord, int offset,  float bias);

uvec4 texture2DArrayOffset(usampler2DArray sampler, vec3 coord, ivec2 offset,  float bias);
