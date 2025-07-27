#pragma once
#include "Passbase.hpp"
#include "../../lib/CFG.hpp"


class SOGE: public _PassBase<SOGE, Module> 
{
public:
  SOGE(Module *m): module(m){ StoreOnlyGlobal.clear();}
  bool run();
private:
  std::vector<Var *> StoreOnlyGlobal;
  Module *module;
  void ScanStoreOnlyGlobal();
};