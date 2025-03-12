#include "../include/Backend/BackendDCE.hpp"

bool BackendDCE::RunImpl()
{
  for (auto b : *func)
    CalCulateSucc(b);
  ///@todo liveness活跃性变量分析
  return;
}

/// @todo
bool BackendDCE::run(RISCVFunction *func)
{
}