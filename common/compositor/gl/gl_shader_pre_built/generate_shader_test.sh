#!/bin/bash

if [ $# -eq 0 ]
  then
    echo "No arguments supplied"
    echo "Using layer_cnt = 1"
    layer_cnt=1
else
    layer_cnt=$1
fi

REQ_FILE="$PWD/require.txt"

while read -r line
do
    shader_test+="$line\n"
done < "$REQ_FILE"

# vertex shader creation
shader_test+="\n"
shader_test+="[vertex shader]\n\n"

shader_test+="#version 300 es\n"
shader_test+="#define LAYER_COUNT "
shader_test+="$layer_cnt\n"

shader_test+="precision mediump int;\n"
shader_test+="uniform vec4 uViewport;\n"
shader_test+="uniform vec4 uLayerCrop[LAYER_COUNT];\n"
shader_test+="uniform mat2 uTexMatrix[LAYER_COUNT];\n"
shader_test+="in vec2 vPosition;\n"
shader_test+="in vec2 vTexCoords;\n"
shader_test+="out vec2 fTexCoords[LAYER_COUNT];\n"
shader_test+="void main() {\n"
shader_test+="  for (int i = 0; i < LAYER_COUNT; i++) {\n"
shader_test+="    vec2 tempCoords = vTexCoords * uTexMatrix[i];\n"
shader_test+="    fTexCoords[i] =\n"
shader_test+="        uLayerCrop[i].xy + tempCoords * uLayerCrop[i].zw;\n"
shader_test+="  }\n"
shader_test+="  vec2 scaledPosition = uViewport.xy + vPosition * uViewport.zw;\n"
shader_test+="  gl_Position =\n"
shader_test+="      vec4(scaledPosition * vec2(2.0) - vec2(1.0), 0.0, 1.0);\n"
shader_test+="}\n"

# fragment shader creation
shader_test+="\n"
shader_test+="[fragment shader]\n\n"
shader_test+="#version 300 es\n"
shader_test+="#define LAYER_COUNT "
shader_test+="$layer_cnt\n"
shader_test+="#extension GL_OES_EGL_image_external : require\n"
shader_test+="precision mediump float;\n"

i=0
while [ $i -lt $layer_cnt ]
do
   shader_test+="uniform samplerExternalOES uLayerTexture$i;\n"
   i=$(( $i + 1 ))
done

shader_test+="uniform float uLayerAlpha[LAYER_COUNT];\n"
shader_test+="uniform float uLayerPremult[LAYER_COUNT];\n"
shader_test+="in vec2 fTexCoords[LAYER_COUNT];\n"
shader_test+="out vec4 oFragColor;\n"
shader_test+="void main() {\n"
shader_test+="  vec3 color = vec3(0.0, 0.0, 0.0);\n"
shader_test+="  float alphaCover = 1.0;\n"
shader_test+="  vec4 texSample;\n"
shader_test+="  vec3 multRgb;\n";

i=0
while [ $i -lt $layer_cnt ]
do
   if [ $i -gt 0 ]
   then
        shader_test+="  if (alphaCover > 0.5/255.0) {\n"
   fi
   shader_test+="  texSample = texture2D(uLayerTexture$i"
   shader_test+=",\n"
   shader_test+="                        fTexCoords[$i"
   shader_test+="]);\n"
   shader_test+="  multRgb = texSample.rgb *\n"
   shader_test+="            max(texSample.a, uLayerPremult[$i"
   shader_test+="]);\n"
   shader_test+="  color += multRgb * uLayerAlpha[$i"
   shader_test+="] * alphaCover;\n"
   shader_test+="  alphaCover *= 1.0 - texSample.a * uLayerAlpha["
   shader_test+="$i];\n";
   i=$(( $i + 1 ))
done

i=1
while [ $i -lt $layer_cnt ]
do
   shader_test+="  }\n"
   i=$(( $i + 1))
done

shader_test+="  oFragColor = vec4(color, 1.0 - alphaCover);\n"
shader_test+="}\n"

outfile_name=hwc_shader_prog_$layer_cnt.shader_test

echo -e "$shader_test" > hwc_shader_prog_$layer_cnt.shader_test

echo "$outfile_name is generated successfully"
