#include "../include/Backend/RISCVLowering.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"
#include "../include/Backend/BuildInFunctionTransform.hpp"
#include "../include/Backend/RISCVMIR.hpp"
#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/PhiElimination.hpp"
#include "../include/Backend/BackendDCE.hpp"
#include "../include/Backend/RegAlloc.hpp"
#include "../include/Backend/PostRACalleeSavedLegalizer.hpp"

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
      std::cerr << "FUNC Lowering failed\n";
    }
  }

  asmprinter->PrintAsm();

  return false;
}

// 函数类型Value
bool RISCVFunctionLowering::run(Function *m)
{
  // 判断是否为内置函数类型
  if (m->GetTag() == Function::BuildIn)
  {
    return false;
  }

  // 预处理（遍历IR指令，识别内置函数调用）
  BuildInFunctionTransform buildin;
  buildin.run(m);

  // IR 映射到 RISCV对象
  auto mfunc = ctx.mapping(m)->as<RISCVFunction>(); // Value匹配RISCVMOperand
  ctx(mfunc);

  // 执行指令选择操作
  // IR中的add映射为RISCV的ADD
  RISCVISel isel(ctx, asmprinter);
  isel.run(m); // 映射
  ///@todo run函数

  // 寄存器分配前：phi函数消除
  // 将SSA形式的IR转换成非SSA形式的IR
  PhiElimination phi(ctx);
  phi.run(m);
  ///@todo run函数下的runonbasicblock

  asmprinter->SetTextSegment(new textSegment(ctx));
  asmprinter->GetData()->GenerateTempvarList(ctx);

  /*
  // 寄存器分配前：基于活跃性分析的死代码消除
  bool modified = true;
  while (modified)
  {
    modified = false;
    BackendDCE dcebefore(mfunc, ctx); // liveness 活跃性变量分析
    modified |= dcebefore.RunImpl();
  }
  */

  // ！！！！！寄存器分配！！！！！
  RegAllocImpl regalloc(mfunc, ctx);
  regalloc.RunGCpass();

  // std::cout << std::flush();考虑要不要

  /*
  // 寄存器分配后：基本块中操作码的死代码消除
  for (auto block : *(ctx.GetCurFunction()))
  {
    for (auto it = block->begin(); it != block->end();)
    {
      auto inst = *it;
      it++;
      // MIR中的获取操作码的类型，与死代码进行比较
      if (inst->GetOpcode() == RISCVMIR::RISCVISA::MarkDead)
        delete inst;
    }
  }

  // 寄存器分配后：活跃性变量分析的死代码消除
  modified = true;
  while (modified)
  {
    modified = false;
    BackendDCE dceafter(ctx.GetCurFunction(), ctx);
    modified |= dceafter.RunImpl();
  }
  */

  /*
  // 检查操作，保证寄存器分配后的寄存器合法
  PostRACalleeSavedLegalizer callee_saved_legalizer;
  callee_saved_legalizer.run(ctx.GetCurFunction()); // 参数说明：当前正在处理的基本块的操作码
  /// @todo run函数算法逻辑
  */

  /*
  // 生成一个函数调用所需要的完整栈帧
  auto &frame = ctx.GetCurFunction()->GetFrame();
  ///@todo RISCVFrame函数栈帧所需类型
  frame->GenerateFrame();
  frame->GenerateFrameHead();
  frame->GenerateFrameTail();
  */

  // 优化
  // ...
  // 优化

  return false;
}