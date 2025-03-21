#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/RISCVAsmPrinter.hpp"

class RISCVISel : public BackEndPass<Function>
{
  RISCVLoweringContext &ctx;
  RISCVAsmPrinter *&asmprinter;

  RISCVMIR *Builder(RISCVMIR::RISCVISA, Instruction *);
  RISCVMIR *Builder_withoutDef(RISCVMIR::RISCVISA, Instruction *);
  RISCVMIR *Builder(RISCVMIR::RISCVISA, std::initializer_list<RISCVMOperand *>);
  RISCVMIR *Builder_withoutDef(RISCVMIR::RISCVISA _isa, std::initializer_list<RISCVMOperand *> list);

  /*
  //后续研究
  void LowerCallInstParallel(CallInst *); // 并行调用场景
  void LowerCallInstCacheLookUp(CallInst *); // 缓存查找
  void LowerCallInstCacheLookUp4(CallInst *); // 4 路缓存查找
  */

  // lowering系列，CFG中
  void InstLowering(Instruction *);
  void InstLowering(LoadInst *);
  void InstLowering(AllocaInst *);
  void InstLowering(CallInst *);
  void InstLowering(RetInst *);
  void InstLowering(CondInst *);
  void InstLowering(UnCondInst *);
  void InstLowering(BinaryInst *);
  void InstLowering(ZextInst *);
  void InstLowering(SextInst *);
  void InstLowering(TruncInst *);
  void InstLowering(MaxInst *);
  void InstLowering(MinInst *);
  void InstLowering(SelectInst *);
  void InstLowering(GepInst *);
  void InstLowering(FP2SIInst *);
  void InstLowering(SI2FPInst *);
  void InstLowering(PhiInst *);

public:
  RISCVISel(RISCVLoweringContext &, RISCVAsmPrinter *&);
  bool run(Function *);
};
