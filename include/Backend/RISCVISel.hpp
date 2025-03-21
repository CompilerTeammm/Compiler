#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVContext.hpp"
#include "../../include/Backend/RISCVAsmPrinter.hpp"

class RISCVISel : public BackEndPass<Function>
{
  RISCVLoweringContext &ctx;
  RISCVAsmPrinter *&asmprinter;

public:
  RISCVISel(RISCVLoweringContext &, RISCVAsmPrinter *&);
  bool run(Function *);
};
