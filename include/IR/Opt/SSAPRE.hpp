#pragma once
#include "Passbase.hpp"
#include "AnalysisManager.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../lib/MyList.hpp"
#include "../Analysis/Dominant.hpp"
class SSAPRE :public _PassBase<SSAPRE,Function>
{
    using TreeNode = DominantTree::TreeNode;
public:
    bool run();
    SSAPRE(Function* _func,DominantTree* _tree): tree(_tree),func(_func){}
    bool PartialRedundancyElimination(Function* func);
private:
    Function* func;
    DominantTree* tree;
};