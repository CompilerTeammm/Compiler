#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include "AnalysisManager.hpp"
#include <vector>
#include <unordered_set>

class CondMerge: public _PassBase<CondMerge, Function>
{
private:    
    Function *func;
    AnalysisManager &AM;
    // DominantTree* tree;
    std::vector<BasicBlock *> DFSOrder;
    std::unordered_set<BasicBlock *> wait_del;

    bool AdjustCondition();
    bool DetectCall(Value *inst, BasicBlock *block, int depth);
    bool RetPhi(BasicBlock *block);
    bool Handle_Or(BasicBlock *cur, BasicBlock *succ, std::unordered_set<BasicBlock *> &wait_del);
    bool Handle_And(BasicBlock *cur, BasicBlock *succ, std::unordered_set<BasicBlock *> &wait_del);
    inline bool Match_Lib_Phi(BasicBlock *curr, BasicBlock *succ, BasicBlock *exit);
    inline bool DetectUserPos(Value *user, BasicBlock *blockpos, Value *val);
    void OrderBlock(BasicBlock *block);
    
public:
    CondMerge(Function *_func,AnalysisManager &_AM) :func(_func),AM(_AM){
        // tree = AM.get<DominantTree>(func);
    }
    ~CondMerge() = default;

    bool run() override;
};