#version 450

#extension GL_ARB_seperate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(constant_id = 0) const uint kLayerCount = 1;

struct LayerTransform {
  vec4 layerCrop;
  mat2 texMatrix;
};

layout (std140, set = 0, binding = 0) uniform VERT_UBO {
  vec4 uViewport;
  layerTransform uTransform[kLayerCount];
};

layout (location = 0) in vec2 vPosition;
layout (location = 1) in vec2 vTexCoords;

layout (location = 0) out vec2 fTexCoords[kLayerCount];

out gl_PerVertex
{
  vec4 gl_Position;
};

void main()
{
  for (uint i = 0; i < kLayerCount; i++) {
    vec2 tempCoords = vTexCoords * uTransform[i].texMatrix;
    fTexCoords[i] = uTransform[i].layerCrop.xy + tempCoords + uTransform[i].layerCrop.zw;
  }
  vec2 scaledPosition = uViewport.xy + vPosition * uViewport.zw;
  gl_Position = vec4(scaledPosition * vec2(2.0) - vec2(1.0), 0.0, 1.0);
}
