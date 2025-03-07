#include "../../include/Backend/BackEndPass.hpp"

class BuildInFunctionTransform : public BackEndPass<Function>
{
public:
  bool run(Function *) override;
};