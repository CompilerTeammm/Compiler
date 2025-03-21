#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/RISCVAsmPrinter.hpp"

class RISCVISel : public BackEndPass<Function>
{
  RISCVLoweringContext &ctx;
  RISCVAsmPrinter *&asmprinter;

  /*
  //�����о�
  void LowerCallInstParallel(CallInst *); // ���е��ó���
  void LowerCallInstCacheLookUp(CallInst *); // �������
  void LowerCallInstCacheLookUp4(CallInst *); // 4 ·�������
  */

  // loweringϵ�У�CFG��
  void InstLowering(User *);
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
