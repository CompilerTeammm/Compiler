#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"


class SSAPRE :public _PassBase<SSAPRE,Function>
{
    // using TreeNode = DominantTree::TreeNode;
public:
    bool run() override ;
    SSAPRE(Function* _func,DominantTree* _tree): tree(_tree),func(_func){}
    bool PartialRedundancyElimination(Function* func);
    ~SSAPRE() = default;
private:
    Function* func;
    DominantTree* tree;
};