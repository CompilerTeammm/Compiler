
#include "../../include/Backend/BackEndPass.hpp"
#include "../../include/Backend/RISCVMIR.hpp"

class PostRACalleeSavedLegalizer : public BackEndPass<RISCVFunction>
{
public:
  bool run(RISCVFunction *) override;
};