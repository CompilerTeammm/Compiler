
#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"

class PhiElimination : public BackEndPass<Function>
{
public:
  bool run(Function *) override;
  PhiElimination(RISCVLoweringContext &_cxt) : cxt(_cxt)
  {
  }

  void runonBasicBlock(BasicBlock *dst);

private:
  RISCVLoweringContext &cxt;
};