//my_stl.hpp 主要是一个辅助工具头文件，提供了一些自定义宏、工具函数，以及控制编译行为的设置。
#pragma once
#include "../include/lib/CFG.hpp"
#include <cassert>//用于断言（assert()）。
#include <vector>//stl动态数组
///@brief 自定义宏，需要什么自己加


//构建支配树相关，用于后续代码优化，死代码消除之类
#define SDOM(x) node[x].sdom //获取x对应结点的sdom
#define MIN_SDOM(x) dsu[x].min_sdom//获取结点最近的sdom的index
#define IDOM(x) node[x].idom//获取结点的idom

//调试相关，在DEBUG模式下会打印信息，在RELEASE模式下编译器会忽略该语句，提高运行速度。
#ifdef DEBUG
#define _DEBUG(x) x
#else
#define _DEBUG(x)
#endif
//编译警告 在GCC编译器上，使用_Pragma()触发警告信息。
##ifdef __GNUC__
#define TO_STRING(x) #x
#define WARN_LOCATION(msg) _Pragma(TO_STRING(GCC warning msg))
#else
#define WARN_LOCATION(msg)
#endif

///@brief 遍历一个function，每一个bb是一个智能指针BasicBlockPtr
#define For_bb_In(function)                                                    \
  assert(dynamic_cast<Function *>(function) &&                                 \
         "incoing must be a function* type");                                  \
  auto &BB = function->GetBasicBlock();                                        \
  for (auto &bb : BB)

/// @brief 遍历一个BB,每个inst是一个User*，
#define For_inst_In(BB)                                                        \
  assert(dynamic_cast<BasicBlock *>(BB) &&                                     \
         "incoing must be a BasicBlock* type");                                \
  for (auto inst : *(BB))

/// @brief 获取指令的操作数
template <typename T> Value *GetOperand(T inst, int i) {
  User *user = dynamic_cast<User *>(inst);
  assert(user);
  return user->Getuselist()[i]->GetValue();
}
/// @brief 获取前驱节点的个数
#define GetPredNum(BB) BB->GetUserListSize()

//拓展标准库stl
///@brief 实现vector元素的pop
template <typename T> void vec_pop(std::vector<T> &vec, int &index) {
    assert(index < vec.size() && "index can not bigger than size");
    vec[index] = vec[vec.size() - 1];
    vec.pop_back();
    index--;
}
///@brief 实现vector没有重复元素
  template <typename T> void PushVecSingleVal(std::vector<T> &vec, T v) {
    auto iter = std::find(vec.begin(), vec.end(), v);
    if (iter != vec.end())
      return;
    vec.push_back(v);
}
///@brief 实现vector弹出最后一个元素  
template <typename T> T PopBack(std::vector<T> &vec) {
    T tmp = vec.back();
    vec.pop_back();
    return tmp;
}
//遍历所有Function，清空BasicBlock并重新编号。
#define PassChangedBegin(curfunc)                                              \
  for (auto &func : module->GetFuncTion()) {                                   \
    curfunc = func.get();                                                      \
    curfunc->bb_num = 0;                                                       \
    curfunc->GetBasicBlock().clear();                                          \
    for (auto bb : *curfunc) {                                                 \
      bb->num = curfunc->bb_num++;                                             \
      curfunc->GetBasicBlock().push_back(bb);                                  \
    }                                                                          \
  }

#define FunctionChange(curfunc)                                                \
  curfunc->bb_num = 0;                                                         \
  curfunc->GetBasicBlock().clear();                                            \
  for (auto bb : *curfunc) {                                                   \
    bb->num = curfunc->bb_num++;                                               \
    curfunc->GetBasicBlock().push_back(bb);                                    \
  }
//运行指定pass
#define RunLevelPass(PassName, curfunc, modified)                              \
  for (int i = 0; i < module->GetFuncTion().size(); i++) {                     \
    curfunc = module->GetFuncTion()[i].get();                                  \
    if(curfunc->tag == Function::Tag::BuildIn)                                 \
      continue;                                                                \
    modified |= RunImpl<PassName>(curfunc, AM);                                \
  }

#define RunEntryFunc(PassName, modified)                                       \
  modified |= RunImpl<PassName>(module->GetMainFunction(), AM);

#define ContinueRunPassOnTest(PassName, curfunc)                               \
  bool modified = true;                                                        \
  while (modified) {                                                           \
    modified = false;                                                          \
    modified = RunImpl<PassName>(curfunc, AM);                                 \
  }