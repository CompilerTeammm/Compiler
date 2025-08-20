#pragma once
#include "BackendPass.hpp"
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "RISCVContext.hpp"

class PhiEliminate : public BackendPassBase
{
    Function* func;
    std::shared_ptr<RISCVContext>& ctx;
    std::map<BasicBlock*,std::map<BasicBlock*,RISCVBlock*>> CopyBlockFinder;
public:
    PhiEliminate(Function* _func,std::shared_ptr<RISCVContext>& _ctx)  :func(_func),ctx(_ctx)  { }
    void runonBasicBlock(BasicBlock* dst);
    void runOnGraph(BasicBlock* pred,BasicBlock* succ,std::vector<std::pair<RISCVOp*,RISCVOp*>>&);
    RISCVBlock* find_copy_block(BasicBlock*,BasicBlock*);
    bool run() override;
};

