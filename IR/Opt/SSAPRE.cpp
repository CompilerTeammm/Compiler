#include "../../include/IR/Opt/SSAPRE.hpp"
#include <unordered_map>

bool SSAPRE::BeginToChange(){
    for(auto& [key,occurList]:exprToOccurList){
        //找出所有使用该表达式的块
        std::set<BasicBlock*> blocksWithExpr;
        for(auto* inst:occurList){
            blocksWithExpr.insert(inst->GetParent());
        }
        
    }
}
bool SSAPRE::PartialRedundancyElimination(Function* func){
    BasicBlock* entryBB = func->GetFront();

    auto* entryNode= tree->getNode(entryBB);//拿到了支配树起始节点
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
    bool hasRedundancy = false;
    for (auto& [key, instList] : occurList) {
        if (instList.size() >= 2) {
            hasRedundancy = true;
        }
    }
    if (hasRedundancy) {
        exprToOccurList = std::move(occurList);//occurlist的内容转移到exprToOccurList
        return BeginToChange();
    }
    return false;
}
bool SSAPRE::run(){
    return PartialRedundancyElimination(func);
}