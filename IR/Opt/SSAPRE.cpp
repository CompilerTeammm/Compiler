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
    using ExprKey = std::string;
    std::unordered_map<ExprKey, std::vector<Instruction*>> occurList;
    std::function<void(DominantTree::TreeNode*)> traverse;
    traverse=[&](DominantTree::TreeNode* node){
        BasicBlock* bb=node->curBlock;
        for(auto inst : *bb){
            if(inst->IsBinary()){
                auto op= inst->GetInstId();
                auto lhs=inst->GetOperand(0);
                auto rhs=inst->GetOperand(1);
                //解决a+b，a*b的问题
                ExprKey leftName=lhs->GetName();
                ExprKey rightName=rhs->GetName();
                //或许可以封装一个IsCommutative（）
                if(op==Instruction::Op::Add||op==Instruction::Op::Mul&&leftName>rightName){
                    std::swap(leftName,rightName);
                }
                ExprKey exprKey=leftName+Instruction::OpToString(op)+rightName;
                occurList[exprKey].push_back(inst);
            }
        }
        for(auto* child:node->idomChild){
            traverse(child);
        }        
    };
    traverse(entryNode);
    for(auto& [key,instList]:occurList){
        if(instList.size()>=2){
            std::cout << "候选冗余表达式: " << key << " 出现于 " << instList.size() << " 个位置\n";
        }
    }
    return change;
}
bool SSAPRE::run(){
    return PartialRedundancyElimination(func);
}