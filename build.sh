#!/bin/bash

# 配置参数
BUILD_DIR="build"
GENERATOR="Ninja"
CMAKE_ARGS="-DCMAKE_BUILD_TYPE=Release"  # 可扩展其他参数

# 清理旧构建
echo "[1/4] 清理旧构建文件..."
if ! rm -rf "$BUILD_DIR"; then
    echo "错误：无法删除 $BUILD_DIR 目录"
    exit 1
fi

# 创建构建目录
echo "[2/4] 创建构建目录..."
if ! mkdir -p "$BUILD_DIR"; then
    echo "错误：无法创建 $BUILD_DIR 目录"
    exit 1
fi

# 生成构建系统
echo "[3/4] 生成构建配置(使用 $GENERATOR)..."
if ! cmake -B "$BUILD_DIR" -G "$GENERATOR" $CMAKE_ARGS; then
    echo "错误:CMake 配置失败"
    exit 1
fi

# 编译项目
echo "[4/4] 开始编译..."
if ! cmake --build "$BUILD_DIR"; then
    echo "错误：编译过程失败"
    exit 1
fi

echo "✅ 构建成功！输出文件在 $BUILD_DIR 目录"
