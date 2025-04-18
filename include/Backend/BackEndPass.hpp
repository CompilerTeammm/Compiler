#pragma once
#include "../../include/lib/CoreClass.hpp"
template <typename T>
class BackEndPass
{
public:
  virtual bool run(T *) = 0;
};