cmake_minimum_required(VERSION 3.11)
project(compiler)

set(CMAKE_MESSAGE_LOG_LEVEL ERROR) 

#C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
# 指定编译类型（可选）
set(CMAKE_BUILD_TYPE Debug )

# if(CMAKE_BUILD_TYPE STREQUAL "Debug")
#     set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined -fno-omit-frame-pointer")
#     set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined")
# endif()

# 包含头文件目录
include_directories(
  include
  include/IR/Analysis
  include/IR/Opt/
  include/lib
  include/lib/ast
  include/MyBackend
  yacc           # 包含parser.hpp所在目录
)

file(GLOB_RECURSE SOURCES
  main.cpp
  IR/Analysis/IDF.cpp
  IR/Analysis/Dominant.cpp
  IR/Analysis/IDF.cpp
  IR/Opt/Mem2reg.cpp
  IR/Opt/MemoryToRegister.cpp
  IR/Opt/DCE.cpp
  IR/Opt/ConstantFold.cpp
  IR/Opt/ConstantProp.cpp
  IR/Opt/GVN.cpp
  IR/Opt/DealOps.cpp

  IR/Opt/SSAPRE.cpp
  IR/Opt/SimplifyCFG.cpp
  
  IR/Opt/LoopUnrolling.cpp
  IR/Analysis/LoopInfo.cpp

  IR/Opt/Inliner.cpp

  MyBackend/*.cpp
  Log/log.cpp

  lib/*.cpp
  yacc/lex.yy.cpp       # 包含lex生成的代码
  yacc/parser.cpp       # 包含yacc生成的代码
)

list(REMOVE_ITEM ALL_HEADERS
    include/IR/Opt/AnalysisManager.hpp
)

add_executable(compiler ${SOURCES})

