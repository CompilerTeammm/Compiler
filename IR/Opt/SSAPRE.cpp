#include "../../include/IR/Opt/SSAPRE.hpp"

inline bool IsCommutative(Instruction::Op op) {
    return op == Instruction::Op::Add || op == Instruction::Op::Mul;
}

Instruction* SSAPRE::FindExpressionInBlock(BasicBlock* bb, const ExprKey& key) {
    for(auto* inst:*bb){
        if(!inst->IsBinary()) continue;
        auto* bin=static_cast<BinaryInst*>(inst);
        Operand l=bin->GetOperand(0);
        Operand r=bin->GetOperand(1);
        if(bin->GetInstId()!=key.op) continue;
        bool match=(l==key.lhs&&r==key.rhs);
        if(!match&&IsCommutative(key.op)){
            match=(l==key.lhs&&r==key.rhs);
            if(match) return bin;
        }
    }
    return nullptr;
}
//可能会存在问题,const删去
std::set<BasicBlock*> SSAPRE::ComputeInsertPoints(DominantTree* tree,std::set<BasicBlock*>& defBlocks){
    IDFCalculator idfCalc(*tree);
    idfCalc.setDefiningBlocks(defBlocks);

    std::vector<BasicBlock*> idfResult;
    idfCalc.calculate(idfResult);

    return std::set<BasicBlock*>(idfResult.begin(),idfResult.end());
}
bool SSAPRE::PartialRedundancyElimination(Function* func){
    //收集所有表达式及位置
    std::unordered_map<ExprKey,std::vector<Instruction*>,ExprKey::Hash> exprToOccurList;
    std::function<void(BasicBlock*)> collectExpr=[&](BasicBlock* bb){
        for(auto* inst:*bb){
            if(!inst->IsBinary()) continue;
            auto* bin=static_cast<BinaryInst*>(inst);
            auto* lhs=bin->GetOperand(0);
            auto* rhs=bin->GetOperand(1);
            Instruction::Op op=bin->GetInstId();
            bool isCommutative = (op == Instruction::Op::Add || op == Instruction::Op::Mul);
            if(isCommutative&&rhs->GetName()>lhs->GetName()){
                std::swap(lhs,rhs);
            }
            ExprKey key{op,lhs,rhs};
            exprToOccurList[key].push_back(bin);
        }
        //遍历后继递归调用
        for(auto* succ:bb->GetNextBlocks()){
            if(tree->dominates(bb,succ)){
                collectExpr(succ);
            }
        }
    };
    collectExpr(func->GetFront());

    //对于重复表达式执行PRE
    for(auto&[key,occurList]:exprToOccurList){
        if(occurList.size()<=1){
            continue;
        }
        //计算插入点
        std::set<BasicBlock*> defBlocks;
        for(auto* inst:occurList){
            defBlocks.insert(inst->GetParent());
        }
        std::set<BasicBlock*> insertPoints = ComputeInsertPoints(tree, defBlocks);
        std::unordered_map<BasicBlock*, Value*> insertValueMap;
        //在插入点插入表达式(有则复用)
        for(auto*bb:insertPoints){
            if(auto* existing=FindExpressionInBlock(bb,key)){
                insertValueMap[bb]=existing->GetDef();
                continue;
            }
            auto* newInst=new BinaryInst(key.lhs,static_cast<BinaryInst::Operation>(key.op),key.rhs);
            auto i=bb->begin();
            i.InsertBefore(newInst);
            insertValueMap[bb]=newInst->GetDef();
        }
        //多前驱插phi
        for(auto* bb:insertPoints){
            if(bb->GetPredBlocks().size()<=1) continue;
            //检查是否已有该phi
            bool alreadyPhi=false;
            for(auto* inst:*bb){
                if(inst->GetInstId()==Instruction::Phi){
                    auto* phi=static_cast<PhiInst*>(inst);
                    if(phi->GetValUseListSize()!=bb->GetPredBlocks().size()) continue;

                    alreadyPhi=true;break;
                }
            }
            if(alreadyPhi) continue;

            std::vector<std::pair<BasicBlock*,Value*>> preds;
            bool allPredOK=true;
            for(auto* pred:bb->GetPredBlocks()){
                Instruction* existing=FindExpressionInBlock(pred,key);
                if(existing){
                    preds.emplace_back(pred,existing->GetDef());
                }else if(insertValueMap.count(pred)){
                    preds.emplace_back(pred,insertValueMap[pred]);
                }else{
                    allPredOK=false;
                    break;
                }
            }
            if(!allPredOK) continue;
            Type* type=key.lhs->GetType();
            auto* phi=new PhiInst(type);
            for(auto& [p,v]:preds){
                phi->addIncoming(v,p);
                auto i=bb->begin();
                i.InsertBefore(phi);
                insertValueMap[bb]=phi->GetDef();
            }
            for(auto* inst:occurList){
                auto* bb=inst->GetParent();
                if(!insertValueMap.count(bb)) continue;
                inst->ReplaceAllUseWith(insertValueMap[bb]);
            }
        }
    }
    return true;
}
bool SSAPRE::run(){
    return PartialRedundancyElimination(func);
}