#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/BuildInFunctionTransform.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/PhiElimination.hpp"
#include "../include/Backend/BackendDCE.hpp"
#include "../include/Backend/RegAlloc.hpp"

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
  // IR中的add映射为RISCV的ADD
  RISCVISel isel(ctx, asmprinter);
  isel.run(m); // 映射
  ///@todo run函数

  // phi函数消除
  // 将SSA形式的IR转换成非SSA形式的IR
  PhiElimination phi(ctx);
  phi.run(m);
  ///@todo run函数下的runonbasicblock

  asmprinter->SetTextSegment(new textSegment(ctx));
  asmprinter->GetData()->GenerateTempvarList(ctx);

  // 死代码消除
  bool modified = true;
  while (modified)
  {
    modified = false;
    BackendDCE dcebefore(mfunc, ctx); // liveness 活跃性变量分析
    modified |= dcebefore.RunImpl();
  }

  // 寄存器分配
  RegAllocImpl regalloc(mfunc, ctx);
  regalloc.RunGCPass();

  // std::cout << std::flush();

  // 遍历基本块，进行死代码消除
  for (auto block : *(ctx.GetCurFunction()))
  {
    for (auto it = block->begin(); it != block->end();)
    {
      auto inst = *it;
      it++;
      if (inst->GetOpcode() == RISCVMIR::RISCVISA::MarkDead)
        delete inst;
    }
  }
  modified = true;
}