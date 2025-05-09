#include "../../include/IR/Opt/SSAPRE.hpp"
#include <unordered_map>

bool SSAPRE::PartialRedundancyElimination(Function* func){
    bool change =false;
    // tree->BuildDominantTree();

    // dh: fixed
    BasicBlock* entryBB = func->GetFront();

    // BasicBlock* entryBB=func->GetBBs()[0].get();//入口块,感觉是个很蠢的寻找方式,先这样吧
    auto* entryNode= tree->getNode(entryBB);//拿到了支配树起始节点
    std::unordered_map<std::string, bool> exprSeen;//储存instruction
    std::function<void(DominantTree::TreeNode*)> traverse;
    traverse=[&](DominantTree::TreeNode* node){
        BasicBlock* bb=node->curBlock;
        for(auto inst : *bb){
            if(inst->IsBinary()){
                auto op= inst->GetInstId();
                auto lhs=inst->GetOperand(0);
                auto rhs=inst->GetOperand(1);
                std::string exprKey=lhs->GetName()+Instruction::OpToString(op)+rhs->GetName();
                //不过需要注意,这里并未考虑a+b和b+a也是冗余的情况,后面修复
                if(exprSeen[exprKey]){
                    std::cout<<"冗余"<<exprKey<<"在基本块:"<<bb->GetName()<<"\n";
                    return change;
                }else{
                    exprSeen[exprKey]=true;
                }
            }
        }
        for(auto* child:node->idomChild){
            traverse(child);
        }        
    };
    traverse(entryNode);
}
bool SSAPRE::run(){
    return PartialRedundancyElimination(func);
}