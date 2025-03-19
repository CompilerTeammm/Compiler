#include "../include/Backend/BuildInFunctionTransform.hpp"
#include "../include/lib/Trival.hpp"

// 遍历基本块和指令，识别函数调用 CALLINst
bool BuildInFunctionTransform::run(Function *func)
{
  for (auto bb : *func)
  {
    for (auto it = bb->begin(); it != bb->end(); it++)
    {
      auto inst = *it;
      if (auto call = dynamic_cast<CallInst *>(inst))
      {
        inst = Trival::BuildInTransform(call);
      }
      it = mylist<BasicBlock, User>::iterator(inst); // 构建回迭代器
    }
  }
  return true;
}