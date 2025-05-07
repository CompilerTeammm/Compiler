#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "AnalysisManager.hpp"

class LoopUnrolling : public _PassBase<LoopUnrolling, Function>
{
public:
  bool run();
  LoopUnrolling(Function *func, AnalysisManager *AM) : _AM(AM), _func(func) {}

private:
  Function *_func;
  AnalysisManager *_AM;
};