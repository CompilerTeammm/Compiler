#include "../../include/IR/Opt/SimplifyCFG.hpp"

bool SimplifyCFG::run() {
    return SimplifyCFGFunction(func);
}

//子优化顺序尝试
bool SimplifyCFG::SimplifyCFGFunction(Function* func){
    bool changed=false;

    //1. 尝试合并空返回块(只处理相同常量返回,无需依赖CFG)
    changed |= mergeEmptyReturnBlocks(func);

    bool localChanged=false;
    do{
        localChanged=false;

        //收集当前基本块列表(避免中途结构变化)
        std::vector<BasicBlock*> blocks;
        for(auto& bb_ptr:func->GetBBs()){
            blocks.push_back(bb_ptr.get());
        }

        //简化条件跳转
        for(auto* bb: blocks){
            localChanged |= simplifyBranch(bb);
        }

        //合并线性块(无phi)
        for (auto* bb : blocks) {
            localChanged |= mergeBlocks(bb);
        }

        // 消除冗余 phi（统一值/已删除 predecessor）
        for (auto* bb : blocks) {
            localChanged |= eliminateTrivialPhi(bb);
        }
        
        //清理CFG中的不可达基本块(否则phi会出错)
        localChanged |= removeUnreachableBlocks(func);

        changed |= localChanged;
    }while(localChanged);//持续迭代直到收敛
}
//辅助函数群:
bool SimplifyCFG::hasOtherRetInst(Function* func,BasicBlock* bb_){
    for(auto& bb_ptr:func->GetBBs()){
        BasicBlock* bb=bb_ptr.get();
        if(bb==bb_) continue;
        if(!bb->reachable) continue;
        for(auto* inst:*bb){
            if(inst->id==Instruction::Op::Ret) return true;
        }
    }
    return false;
}
// 判断指令是否为副作用调用
bool SimplifyCFG::hasSideEffect(Instruction* inst){
    if(inst->IsCallInst()){
        std::string callee=inst->GetName();
        //根据实际副作用函数名调整判断条件
        if(callee.find("_sysy")!=std::string::npos||callee=="putint"){
            return true;
        }
    }
    return false;
}
// 判断基本块是否含有副作用调用
bool SimplifyCFG::blockHasSideEffect(BasicBlock* bb) {
    for (auto it = bb->begin(); it != bb->end(); ++it) {
        Instruction* inst = *it;
        if (hasSideEffect(inst)) return true;
    }
    
    return false;
}

// 判断是否唯一返回块
bool SimplifyCFG::isOnlyRetBlock(Function* func, BasicBlock* bb) {
    int retCount = 0;
    for (auto& b : func->GetBBs()) {
        if (b->GetLastInsts()->IsTerminateInst() && b->GetLastInsts()->id==Instruction::Op::Ret){
            ++retCount;
        }
    }
    return retCount == 1 && bb->GetLastInsts()->IsTerminateInst() && bb->GetLastInsts()->id==Instruction::Op::Ret;
}

//合并空返回块(no phi)(实际上是合并所有返回相同常量值的返回块)
bool SimplifyCFG::mergeEmptyReturnBlocks(Function* func){
    auto& BBs=func->GetBBs();
    std::vector<BasicBlock*> ReturnBlocks;
    std::optional<int> commonRetVal;//optional用于标识一个值要么存在要么不存在(可选值)
    //记录目标常量返回值

    //收集所有返回指令,返回值需要是整数常量且值相同的块
    for(auto& bbPtr:BBs){
        BasicBlock* bb=bbPtr.get();
        if(bb->Size()!=1) continue;
        //基本块内只有一条指令(ret)
        Instruction* lastInst=bb->GetLastInsts();
        if(!lastInst || lastInst->id!=Instruction::Op::Ret) continue;
        
        auto* retInst=dynamic_cast<RetInst*>(lastInst);
        if (!retInst || retInst->GetOperandNums() != 1) continue;

        Value* retVal=retInst->GetOperand(0);
        auto* c=dynamic_cast<ConstIRInt*>(retVal);
        if(!c) continue;
        int val=c->GetVal();
        if(!commonRetVal.has_value()){
            commonRetVal =val;
        }
        if(val!=commonRetVal.value()){
            continue;
        }

        //跳过含副作用的块
        if (blockHasSideEffect(bb)) {
            std::cerr << "Skipped return block with side effects: " << bb->GetName() << "\n";
            continue;
        }

        //跳过唯一合法ret块
        if (isOnlyRetBlock(func,bb)) {
            std::cerr << "Skipped only return block: " << bb->GetName() << "\n";
            continue;
        }

        ReturnBlocks.push_back(bb);
    }
    //合并空ret块
    if(ReturnBlocks.size()==1){
        std::cerr << "Only one return block found: " << ReturnBlocks[0]->GetName() << ", skipped merging.\n";
        return false;
    }
    if(ReturnBlocks.size()==0){
        std::cerr << "No return blocks found.\n";
        return false;
    }
    std::cerr<<"Found"<<ReturnBlocks.size()<<" return blocks with common return value: "<<commonRetVal.value()<<"\n";

    //选定第一个作为公共返回块
    BasicBlock* commonRet=ReturnBlocks.front();

    bool changed=false;
    //重定向其他返回块的前驱到commonRet
    for(size_t i=1;i<ReturnBlocks.size();++i){
        BasicBlock* redundant=ReturnBlocks[i];
        std::cerr << "Removed redundant return block: " << redundant->GetName() << "\n";
        //重定向所有前驱块的后继指针从redundant到commonRet
        for(auto* pred: redundant->GetPredBlocks()){
            if(pred->Size()==0) continue;
            auto term=pred->GetLastInsts();
            if(!term) continue;

            bool replaced=false;
            //替换terminator的operand
            for(int i=0;i<term->GetOperandNums();++i){
                if(term->GetOperand(i)==redundant){
                    term->SetOperand(i, commonRet);
                    replaced=true;
                    std::cerr << "    [Redirected] " << pred->GetName() << " -> " << commonRet->GetName() << "\n";
                }
            }
            if(replaced){
                pred->RemoveNextBlock(redundant);
                pred->AddNextBlock(commonRet);
                commonRet->AddPredBlock(pred);
            }
        }
        //从函数中移除
        std::cerr<< "Removed redundant return block: "<<redundant->GetName()<<"\n";
        func->RemoveBBs(redundant);
        changed=true;
    }
    return changed;
}

//删除不可达基本块(记得要把phi引用到的也进行处理)
bool SimplifyCFG::removeUnreachableBlocks(Function* func){
    std::unordered_set<BasicBlock*> reachable;//存储可达块
    std::stack<BasicBlock*> bbstack;

    auto entry=func->GetFront();
    bbstack.push(entry);
    reachable.insert(entry);

    //DFS
    while(!bbstack.empty()){
        BasicBlock* bb=bbstack.top();
        bbstack.pop();
        for(auto& succ:bb->GetNextBlocks()){
            if(reachable.insert(succ).second){
                bbstack.push(succ);
            }
        }
    }

    bool changed=false;
    //遍历所有bb,移除不可达者
    auto& BBList=func->GetBBs();
    for(auto it=BBList.begin();it!=BBList.end();){
        BasicBlock* bb=it->get();
        if(reachable.count(bb)==0){

            //检查该块中是否有唯一的ret
            bool containsRet=false;
            for(auto* inst:*bb){
                if(inst->id==Instruction::Op::Ret){
                    containsRet=true;
                    break;
                }
            }
            if(containsRet && !hasOtherRetInst(func,bb)){
                std::cerr << "WARNING: This is the only return block. Keeping: " << bb->GetName() << "\n";
                ++it;
                continue;
            }
            
            std::cerr << "Erasing unreachable block: " << bb->GetName() << std::endl;
            //清理其产生的值被使用的地方
            for(auto i=bb->begin();i!=bb->end();++i){
                Instruction* inst=*i;
                inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
            }
            //移除phi中引用到这个bb的分支
            for(auto succ:bb->GetNextBlocks()){
                for(auto it=succ->begin();it!=succ->end();++it){
                    if(auto phi=dynamic_cast<PhiInst*>(*it)){
                        phi->removeIncomingFrom(bb);
                    }else{
                        break;
                    }
                }
            }
            for(auto pred:bb->GetPredBlocks()){
                pred->RemoveNextBlock(bb);
            }
            for(auto succ:bb->GetNextBlocks()){
                succ->RemovePredBlock(bb);
            }
            it=BBList.erase(it);
            changed=true;
        }else{
            ++it;
        }
    }
    return changed;
}


bool SimplifyCFG::simplifyBranch(BasicBlock* bb){
    if(bb->Size()==0){
        return false;
    }
    //获取基本块最后一条指令
    Instruction* lastInst=bb->GetBack();

    //判断是否条件跳转指令
    bool is_cond_branch = lastInst && lastInst->id==Instruction::Op::Cond;
    if(!is_cond_branch){
        return false;
    }
    //获取条件操作数和两个基本块
    Value* cond=lastInst->GetOperand(0);
    BasicBlock* trueBlock=dynamic_cast<BasicBlock*>(lastInst->GetOperand(1));
    BasicBlock* falseBlock=dynamic_cast<BasicBlock*>(lastInst->GetOperand(2));
    //确认`目标基本块合法
    if(!trueBlock||!falseBlock){
        return false;
    }
    //判断条件是否是常量函数
    auto* c=dynamic_cast<ConstIRBoolean*>(cond);
    if(!c){
        std::cerr << "Not a constant condition\n";
        return false;
    }
    BasicBlock* targetBlock=c->GetVal() ? trueBlock:falseBlock;
    //创建无条件跳转指令,替换原条件跳转指令
    auto oldInst=bb->GetLastInsts();
    bb->erase(oldInst);
    Instruction* uncondBr=new UnCondInst(targetBlock);
    bb->push_back(uncondBr);

    //更新CFG
    bb->RemoveNextBlock(trueBlock);
    bb->RemoveNextBlock(falseBlock);
    trueBlock->RemovePredBlock(bb);
    falseBlock->RemovePredBlock(bb);

    bb->AddNextBlock(targetBlock);
    targetBlock->AddPredBlock(bb);


    std::cerr << "Simplified to: br label %" << targetBlock->GetName() << "\n";
    return true;
}

//合并基本块(no phi)
//不过只能合并线性路径,后面要补充
bool SimplifyCFG::mergeBlocks(BasicBlock* bb){
    //获取后继块
    if(bb->GetNextBlocks().size()!=1){
        return false;
    }
    auto succ=bb->GetNextBlocks()[0];
    //后继不能是自身,避免死循环
    if(succ==bb){
        return false;
    }
    //判断succ是否只有bb一个前驱
    if(succ->GetPredBlocks().size()!=1||succ->GetPredBlocks()[0]!=bb){
        return false;
    }

    //ok,那满足条件,合并
    //移除bb中的terminator指令(一般是br)
    if(bb->Size()!=0 && bb->GetBack()->IsTerminateInst()){
        bb->GetBack()->EraseFromManager();
    }
    while (succ->Size() > 0) {
        Instruction *inst = succ->pop_front();
        bb->push_back(inst);
    }
    //更新CFG

    //succ的后继接到bb上
    auto nexts=succ->GetNextBlocks();
    for(auto succsucc:nexts){
        succsucc->RemovePredBlock(succ);
        succsucc->AddPredBlock(bb);
        bb->AddNextBlock(succsucc);
    }

    //断开bb与succ
    bb->RemoveNextBlock(succ);
    succ->RemovePredBlock(bb);

    func->RemoveBBs(succ);
    return true;
}

//消除无意义phi
bool SimplifyCFG::eliminateTrivialPhi(BasicBlock* bb){
    bool changed=false;

    //遍历当前基本块中所有指令
    for(auto it=bb->begin();it!=bb->end();){
        Instruction* inst=*it;

        //isphi?
        if(inst->id==Instruction::Op::Phi){
            Value* same=nullptr;
            bool all_same=true;

            //遍历所有phi的输入值
            for(size_t i=0;i<inst->GetOperandNums();i+=2){
                Value* val=inst->GetOperand(i);
                if(!same){
                    same=val;
                }else if(val!=same){
                    all_same=false;
                    break;
                }
            }

            //所有输入值相同,可以替换
            if(all_same&&same){
                inst->ReplaceAllUseWith(same);
                
                auto to_erase=it;
                ++it;
                bb->erase(*to_erase);
                
                changed=true;
                continue;
            }
        }
        ++it;
    }
    return changed;
}