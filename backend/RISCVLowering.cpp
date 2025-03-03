#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"

extern std::string asmoutput_path;
RISCVAsmPrinter *asmprinter = nullptr;

// 全局参数
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