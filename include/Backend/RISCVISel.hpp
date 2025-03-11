#include "../../include/Backend/BackEndPass.hpp"

class RISCVISel : public BackEndPass<Function>
{
public:
  RISCVISel(RISCVLoweringContext &_ctx, RISCVAsmPrinter *&asmprinter)
  {
  }

  bool run(Function *);
};
