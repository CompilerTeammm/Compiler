#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include "AnalysisManager.hpp"

class CondMerge: public _PassBase<CondMerge, Function>
{
private:    
    Function *func;
    AnalysisManager &AM;
    

    bool TryMergeCondOr(BasicBlock* bb,BasicBlock* trueBlock,BasicBlock* falseBlock);
    
public:
    CondMerge(Function *_func,AnalysisManager &_AM) :func(_func),AM(_AM){
    }
    ~CondMerge() = default;

    bool run() override;
};