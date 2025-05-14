#include "../../include/IR/Opt/SSAPRE.hpp"

Instruction* SSAPRE::findExpressionInBlock(BasicBlock* bb, const ExprKey& key) {
    auto it = exprToOccurList.find(key);
    if (it == exprToOccurList.end()||it->second.empty()) return nullptr;

    for (auto* inst : it->second) {
        if (inst->GetParent() == bb)
            return inst;
    }
    return nullptr;
}

std::set<BasicBlock*> SSAPRE::ComputeInsertPoints(DominantTree* tree,const std::set<BasicBlock*>& blocksWithExpr){
    IDFCalculator idfCalc(*tree);
    std::set<BasicBlock*> defBlocks = blocksWithExpr;
    idfCalc.setDefiningBlocks(defBlocks);

    std::vector<BasicBlock*> IDFBlocks;
    idfCalc.calculate(IDFBlocks);

    return std::set<BasicBlock*>(IDFBlocks.begin(),IDFBlocks.end());
}
bool SSAPRE::BeginToChange(){
    bool changed=false;
    for(auto& [key,occurList]:exprToOccurList){
        //找出所有使用该表达式的块
        std::set<BasicBlock*> blocksWithExpr;
        for(auto* inst:occurList){
            blocksWithExpr.insert(inst->GetParent());
        }

        //计算DF支配边界,找到插入点
        std::set<BasicBlock*> insertPoints=ComputeInsertPoints(tree,blocksWithExpr);

        //在插入点插入表达式,生成新的SSA临时变量
        assert(!occurList.empty());
        auto* firstInst=dynamic_cast<BinaryInst*>(occurList[0]);
        if(!firstInst) continue;
        //调试信息
        if (!occurList[0]) {
            std::cerr << "nullptr in occurList for key: " << key << "\n";
            continue;
        }

        Operand lhs = firstInst->GetOperand(0);  // 第一个操作数
        Operand rhs = firstInst->GetOperand(1);  // 第二个操作数
        auto op = firstInst->GetOp(); //运算类型
        auto tp=firstInst->GetType(); 
        //记录插入点及其对应的新定义值（为后续替换做准备）
        std::unordered_map<BasicBlock*, Operand> insertPointToNewValue;
        for(auto* bb:insertPoints){
            // auto newInst=new BinaryInst(lhs,op,rhs);
            // auto i=bb->begin();
            // i.InsertBefore(newInst);
            // auto result=newInst->GetDef();
            // insertPointToNewValue[bb]=result;
            //得插入phi函数,因为已经是ssa形式了
            auto* phi=new PhiInst(tp);//新建一个phi函数
            for(auto* pred:bb->GetPredBlocks()){
                // Operand val;
                // Instruction* found=findExpressionInBlock(pred,key);
                // if(found){
                //     val=found->GetDef();
                // }else if(insertPointToNewValue.count(pred)){
                //     val=insertPointToNewValue[pred];
                // }else{
                //     val=UndefValue::Get(tp);
                // }
                if(!pred){
                    std::cerr<<"Invalid predecessor block.\n";
                    continue;
                }
                Operand val=UndefValue::Get(tp);
                Instruction* found=findExpressionInBlock(pred,key);
                if(found){
                    val=found->GetDef();
                }else{//递归查找pred块的插入点
                    auto it=insertPointToNewValue.find(pred);
                    if(it!=insertPointToNewValue.end()){
                        val=it->second;
                    }
                }
                phi->addIncoming(val, pred);
            }
            auto i=bb->begin();
            i.InsertBefore(phi);
            auto result=phi->GetDef();
            if (!result) {
            std::cerr << "phi->GetDef() returned nullptr at insert point.\n";
            continue;
            }

            insertPointToNewValue[bb]=result;
        }
        //替换冗余表达式
        for(auto* inst:occurList){
            for(auto& use: inst->GetUserUseList()){
                if(use->GetValue()==inst){
                    // use->SetValue(insertPointToNewValue[inst->GetParent()]);
                    // Value* newVal=insertPointToNewValue[inst->GetParent()];
                    // use->SetValue(newVal);
                    // inst->ReplaceAllUseWith(insertPointToNewValue[inst->GetParent()]);
                    //调试信息
                    auto it=insertPointToNewValue.find(inst->GetParent());
                    if(it!=insertPointToNewValue.end()){
                        inst->ReplaceAllUseWith(it->second);
                    }else{
                        std::cerr<< "No replacement value for inst in "<<inst->GetParent()->GetName()<<"\n";
                    }
                }
            }
        }
        changed=true;
    }
    return changed;
}
bool SSAPRE::PartialRedundancyElimination(Function* func){
    BasicBlock* entryBB = func->GetFront();

    auto* entryNode= tree->getNode(entryBB);//拿到了支配树起始节点


    std::unordered_map<ExprKey, std::vector<Instruction*>> occurList{};//需要显式初始化？


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
                bool IsCommutative = (op == Instruction::Op::Add || op == Instruction::Op::Mul);
                if(IsCommutative&&leftName>rightName){
                    std::swap(leftName,rightName);
                }
                ExprKey exprKey=leftName+Instruction::OpToString(op)+rightName;
                //调试信息
                if (leftName.empty() || rightName.empty()) {
                    std::cerr << "Empty operand name in expression: " << inst->GetName()<< "\n";
                    continue;
                }

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