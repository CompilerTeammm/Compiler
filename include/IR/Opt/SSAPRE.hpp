#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "IDF.hpp"
#include <unordered_map>
#include <string>
class SSAPRE :public _PassBase<SSAPRE,Function>
{
    // using TreeNode = DominantTree::TreeNode;
private:
    using ExprKey = std::string;
    Function* func;
    DominantTree* tree;
    std::unordered_map<ExprKey, std::vector<Instruction*>> exprToOccurList;
public:
    bool run() override ;
    SSAPRE(Function* _func,DominantTree* _tree): tree(_tree),func(_func){}
    ~SSAPRE() = default;
    bool PartialRedundancyElimination(Function* func);
    //实际上只是检测是否有部分冗余，接下来交给BeginToChange
    bool BeginToChange();
    std::set<BasicBlock*> ComputeInsertPoints(std::set<BasicBlock*>);
    Instruction* findExpressionInBlock(BasicBlock*, const ExprKey&);
};