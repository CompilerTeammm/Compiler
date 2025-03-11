#include "../include/Backend/RISCVISel.hpp"
#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"

// ≥ı ºªØ RISCVLoweringContext ∫Õ RISCVAsmPrinter
RISCVISel::RISCVISel(RISCVLoweringContext &_ctx, RISCVAsmPrinter *&asmprinter) : ctx(_ctx), asmprinter(asmprinter)
{
}

bool RISCVISel::run(Funciton *m)
{
}