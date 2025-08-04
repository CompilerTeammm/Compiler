#pragma once
#include "../../lib/CFG.hpp"
#include "../../lib/CoreClass.hpp"
#include "Dominant.hpp"
#include "../Opt/AnalysisManager.hpp"
#include <cstddef>
#include <unordered_map>
enum InstTy {
    Load = 114,
    Gep = 514,
  };

//基于哈希值的别名分析（AliasAnalysis）实现
class AliasAnalysis : public _AnalysisBase<AliasAnalysis, Function> {
public:
  explicit AliasAnalysis(Function *_func) : func(_func) {}
  void *GetResult(Function *func);
  static size_t GetHash(Value *val);
  std::set<Value *> FindHashVal(size_t hs);

private:
  void run();
  Function *func;
  std::unordered_map<size_t, std::set<Value *>> AliasTable;
};