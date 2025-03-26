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
  // Future work
  void LowerCallInstParallel(CallInst *); // Parallel call scenario
  void LowerCallInstCacheLookUp(CallInst *); // Cache lookup
  void LowerCallInstCacheLookUp4(CallInst *); // 4-way cache lookup
  */

  // In the lowering phase within the Control Flow Graph(CFG)
  void InstLowering(Instruction *); //
  void InstLowering(LoadInst *);    //

  /// @todo
  void InstLowering(AllocaInst *); //
  void InstLowering(CallInst *);   //

  void InstLowering(RetInst *);    //
  void InstLowering(CondInst *);   //
  void InstLowering(UnCondInst *); //
  void InstLowering(BinaryInst *); //
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
