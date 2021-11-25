#!/bin/sh

#  shader_compile.sh
#  vulkan_engine
#
#  Created by Lorenzo Bozza on 05/11/21.
#  

GLSLC="/Users/lorenzo/VulkanSDK/1.2.189.0/macOS/bin/glslc"

SHADERS="/Users/lorenzo/myGitRepos/vulkan_engine/source/shaders/"

DEBUG="/Users/lorenzo/Library/Developer/Xcode/DerivedData/vulkan_engine-cueksqjdkbojfycewoeoqgcxstgc/Build/Products/Debug/"

cd ${SHADERS}
for i in *
do
    ${GLSLC} "${i}" -o "${DEBUG}${i}.spv"
done
