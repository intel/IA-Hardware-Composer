#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(constant_id = 0) const uint kLayerCount = 1;

struct LayerBlend {
  float alpha;
  float premult;
};

layout (std140, set = 0, binding = 1) uniform FRAG_UBO {
  LayerBlend uLayerBlend[kLayerCount];
};

layout (set = 0, binding = 2) uniform sampler2D uLayerTextures[kLayerCount];
layout (location = 0) in vec2 fTexCoords[kLayerCount];
layout (location = 0) out vec4 oFragColor;

void main() {
  vec3 color = vec3(0.0, 0.0, 0.0);
  float alphaCover = 1.0;
  vec4 texSample;
  vec3 multRgb;

  for (uint i = 0; i < kLayerCount; i++) {
    if (alphaCover > 0.5/255.0) {
      texSample = texture(uLayerTextures[i], fTexCoords[i]);
      multRgb = texSample.rgb * max(texSample.a, uLayerBlend[i].premult);
      color += multRgb * uLayerBlend[i].alpha * alphaCover;
      alphaCover *= 1.0 - texSample.a * uLayerBlend[i].alpha;
    }
  }
  oFragColor = vec4(color, 1.0 - alphaCover);
}

