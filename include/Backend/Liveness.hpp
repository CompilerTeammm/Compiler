#include "../../include/Backend/RISCVMIR.hpp"

using BlockInfo = Liveness;

class Liveness
{
public:
  Liveness(RISCVFunction *f)
  {
  }
  void CalCulateSucc(RISCVBasicBlock *block);
};

void BlockInfo::CalCulateSucc(RISCVBasicBlock *block)
{
}