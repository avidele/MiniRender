#!/bin/bash
# 编译着色器的脚本

# 确保Vulkan SDK环境变量已设置
GLSLC="/Users/avidel/VulkanSDK/1.4.309.0/macOS/bin/glslc"

if [ ! -f "$GLSLC" ]; then
  echo "无法找到glslc编译器：$GLSLC"
  exit 1
fi

echo "正在编译顶点着色器..."
$GLSLC -fshader-stage=vertex vert.glsl -o vert.spv

echo "正在编译片段着色器..."
$GLSLC -fshader-stage=fragment frag.glsl -o frag.spv

if [ $? -eq 0 ]; then
  echo "着色器编译成功！"
else
  echo "着色器编译失败！"
  exit 1
fi