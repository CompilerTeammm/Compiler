#include "BackEndPass.hpp"
#include "../../include/lib/Module.hpp"

// 定义 LLVM IR 到 RISCV 转换的类
class RISCVModuleLowing : BackEndPass<Module>
{
  void operator()(RISCVFunction *); // 初始化或处理整个函数
  RISCVFunction *&GetCurFunction(); // 获取当前正在处理的函数
};