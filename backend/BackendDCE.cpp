#include "../include/Backend/BackendDCE.hpp"

bool BackendDCE::RunImpl()
{
  for (auto b : *func)
    CalCulateSucc(b);
  ///@todo liveness��Ծ�Ա�������
  return;
}

/// @todo
bool BackendDCE::run(RISCVFunction *func)
{
}