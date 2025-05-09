#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"
#include "AnalysisManager.hpp"
#include "../Analysis/LoopInfo.hpp"

class LoopUnrolling : public _PassBase<LoopUnrolling, Function>
{
public:
  LoopUnrolling(Function *func, AnalysisManager *AM) : _AM(AM), _func(func) {}
  bool run();

private:
  Function *_func;
  AnalysisManager *_AM;
};