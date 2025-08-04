#include "../../include/IR/Analysis/AliasAnalysis.hpp"
#include <cassert>
#include <functional>

void AliasAnalysis::run(){
    for (auto bb_it = func->begin(); bb_it != func->end(); ++bb_it){
        auto bb = *bb_it;
        for (auto inst_it = bb->begin(); inst_it != bb->end(); ++inst_it){
            auto inst = *inst_it;
            if (!inst) continue;
            if (auto ld = dynamic_cast<LoadInst *>(inst)) {
                auto target = ld->GetOperand(0);
                auto hash = std::hash<Value *>()(target);
                hash = std::hash<int>()(Load) + hash;
                AliasTable[hash].insert(ld);
        
              } else if (auto gep = dynamic_cast<GepInst *>(inst)) {
                auto hash = std::hash<int>()(Gep);
                for (int i = 0; i < gep->GetOperandNums(); i++) {
                  hash += std::hash<Value *>()(gep->GetOperand(i));
                }
                AliasTable[hash].insert(gep);
              }
        }
    }
}

size_t AliasAnalysis::GetHash(Value *val) {
    if (auto ld = dynamic_cast<LoadInst *>(val)) {
      auto target = ld->GetOperand(0);
      auto hash = std::hash<Value *>()(target);
      hash = std::hash<int>()(Load) + hash;
      return hash;
    } else if (auto gep = dynamic_cast<GepInst *>(val)) {
      auto hash = std::hash<int>()(Gep);
      for (int i = 0; i < gep->GetOperandNums(); i++) {
        hash += ((std::hash<Value *>()(gep->GetOperand(i)) << (i + 3)) * (i + 101));
      }
      return hash;
    }
    return 0;
}

void *AliasAnalysis::GetResult(Function *func) {
    AliasTable.clear();
    run();
    return this;
}

std::set<Value *> AliasAnalysis::FindHashVal(size_t hs) {
    auto it = AliasTable.find(hs);
    return it != AliasTable.end() ? it->second : std::set<Value*>{};
}