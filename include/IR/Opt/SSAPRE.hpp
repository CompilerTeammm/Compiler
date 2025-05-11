#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "IDF.hpp"
#include <unordered_map>

class SSAPRE :public _PassBase<SSAPRE,Function>
{
    // using TreeNode = DominantTree::TreeNode;
public:
    bool run() override ;
    SSAPRE(Function* _func,DominantTree* _tree): tree(_tree),func(_func){}
    bool PartialRedundancyElimination(Function* func);
    //实际上只是检测是否有部分冗余，接下来交给BeginToChange
    bool BeginToChange();
    std::set<BasicBlock*> ComputeInsertPoints(std::set<BasicBlock*>);
    // Instruction* InsertExprAt(BasicBlock*,Instruction::Op,Value*,Value*);
    ~SSAPRE() = default;
private:
    Function* func;
    DominantTree* tree;
    using ExprKey = std::string;
    std::unordered_map<ExprKey, std::vector<Instruction*>> exprToOccurList;
};