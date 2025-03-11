#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/BuildInFunctionTransform.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVISel.hpp"

extern std::string asmoutput_path;
RISCVAsmPrinter *asmprinter = nullptr;

// 全局参数Value
void RISCVModuleLowering::LowerGlobalArgument(Module *m)
{
  asmprinter = new RISCVAsmPrinter(asmoutput_path, m, ctx);
}

bool RISCVModuleLowering::run(Module *m)
{
  LowerGlobalArgument(m);
  RISCVFunctionLowering funclower(ctx, asmprinter);
  auto &funcS = m->GetFuncTion();

  for (auto &func : funcS)
  {
    if (funclower.run(func.get())) // 这里的逻辑有待考察
    {
      func->print();
      std::corr << "FUNC Lowering failed\n";
    }
  }

  asmprinter->printAsm();

  return false;
}

// 函数类型Value
bool RISCVFunctionLowering::run(Function *m)
{
  // 判断是否为内置函数类型
  if (m->GetTag() == Functin::BuildIn)
  {
    return false;
  }

  // 预处理（遍历IR指令，识别内置函数调用）
  BuildInFunctionTransform buildin;
  buildin.run(m);

  ///@todo 实现run函数，IR中内置类型函数转换

  // IR 映射到 RISCV对象
  auto mfunc = ctx.mapping(m)->as<RISCVFunction>(); // Value匹配RISCVMOperand
  ///@todo 实现RISCVLoweringContext中的mapping查找
  ///@todo RISCVMOperand中的逻辑，一个实例化？
  ctx(mfunc); // 重载了

  // 执行指令选择操作
  RISCVISel isel(ctx, asmprinter);
  isel.run(m); // 映射
  // IR中的add映射为RISCV的ADD
}