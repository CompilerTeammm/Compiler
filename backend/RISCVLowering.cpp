#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/BuildInFunctionTransform.hpp"
#include "../include/Backend/RISCVMIR.hpp"

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
  BuildInFunctionTransform buildin;
  buildin.run(m);

  ///@todo 实现run函数，IR中内置类型函数转换

  auto mfunc = ctx.mapping(m)->as<RISCVFunction>(); // Value匹配RISCVMOperand
  ctx(mfunc);                                       // 重载了
}