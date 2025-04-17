
#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"

class PhiElimination : public BackEndPass<Function>
{

  RISCVLoweringContext &cxt;
  std::map<BasicBlock *, std::map<BasicBlock *, RISCVBasicBlock *>> CopyBlockFinder;
  RISCVBasicBlock *find_copy_block(BasicBlock *, BasicBlock *);
  void runOnGraph(BasicBlock *pred, BasicBlock *succ, std::vector<std::pair<RISCVMOperand *, RISCVMOperand *>> &);
  void runonBasicBlock(BasicBlock *dst);

public:
  bool run(Function *) override;
  PhiElimination(RISCVLoweringContext &_cxt) : cxt(_cxt)
  {
  }
};