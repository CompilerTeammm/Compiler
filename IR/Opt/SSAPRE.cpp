#include "../../include/IR/Opt/SSAPRE.hpp"

bool SSAPRE::PartialRedundancyElimination(Function* func){
    bool change =false;
    DominantTree domTree(func);
    domTree.BuildDominantTree();

    // dh: fixed
    BasicBlock* entry = func->GetFront();

    BasicBlock* entryBB=func->GetBBs()[0].get();//入口块,感觉是个很蠢的寻找方式,先这样吧
    auto* entryNode= domTree.getNode(entryBB);//拿到了支配树起始节点

    std::function<void(DominantTree::TreeNode*)> traverse;
    traverse=[&](DominantTree::TreeNode* node){
        BasicBlock* bb=node->curBlock;
    }
    for(auto& inst:bb->get)
}
bool SSAPRE::run(){
    return PartialRedundancyElimination(func);
}