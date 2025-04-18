#pragma once
#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVAsmPrinter.hpp"

void LowerFormalArguments(Function *func, RISCVLoweringContext &ctx);
class RISCVISel : public BackEndPass<Function>
{
  RISCVLoweringContext &ctx;
  RISCVAsmPrinter *&asmprinter;

  RISCVMIR *Builder(RISCVMIR::RISCVISA, Instruction *);
  RISCVMIR *Builder_withoutDef(RISCVMIR::RISCVISA, Instruction *);
  RISCVMIR *Builder(RISCVMIR::RISCVISA, std::initializer_list<RISCVMOperand *>);
  RISCVMIR *Builder_withoutDef(RISCVMIR::RISCVISA _isa, std::initializer_list<RISCVMOperand *> list);

  // In the lowering phase within the Control Flow Graph(CFG)
  void InstLowering(Instruction *); //
  void InstLowering(LoadInst *);    //

  void InstLowering(AllocaInst *); //
  void InstLowering(CallInst *);   //
  void InstLowering(StoreInst *);  //

  void InstLowering(RetInst *);    //
  void InstLowering(CondInst *);   //
  void InstLowering(UnCondInst *); //
  void InstLowering(BinaryInst *); //
  void InstLowering(ZextInst *);   //
  void InstLowering(SextInst *);   //
  void InstLowering(TruncInst *);  //
  void InstLowering(MaxInst *);    //
  void InstLowering(MinInst *);    //
  void InstLowering(SelectInst *); //
  void InstLowering(GepInst *);    //
  void InstLowering(FP2SIInst *);  //
  void InstLowering(SI2FPInst *);  //
  void InstLowering(PhiInst *);    //

  void LowerCallInstParallel(CallInst *);
  void LowerCallInstCacheLookUp(CallInst *);
  void LowerCallInstCacheLookUp4(CallInst *);

  void condition_helper(BinaryInst *);

public:
  RISCVISel(RISCVLoweringContext &, RISCVAsmPrinter *&);
  bool run(Function *);
};
