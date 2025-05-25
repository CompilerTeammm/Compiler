#include "../../include/IR/Opt/SimplifyCFG.hpp"

bool SimplifyCFG::run() {
    return SimplifyCFGFunction(func);
}

bool SimplifyCFG::SimplifyCFGFunction(Function* func){
    bool changed=false;

    //function子优化
    changed |= removeUnreachableBlocks(func);
    changed |= mergeEmptyReturnBlocks(func);
    //basicblock子优化
    std::vector<BasicBlock*> blocks;
    for(auto& bb_ptr:func->GetBBs()){
        blocks.push_back(bb_ptr.get());//从shared_ptr提取裸指针
    }
    for(auto* bb:blocks){
        changed|=SimplifyCFGBasicBlock(bb);
    }
    return changed;
}

bool SimplifyCFG::SimplifyCFGBasicBlock(BasicBlock* bb){
    bool changed=false;
    changed |=mergeBlocks(bb);
    changed |=simplifyBranch(bb);
    changed |=eliminateTrivialPhi(bb);

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
            //移除phi中引用到这个bb的分支
            for(auto succ:bb->GetNextBlocks()){
                for(auto it=succ->begin();it!=succ->end();++it){
                    if(auto phi=dynamic_cast<PhiInst*>(*it)){
                        phi->removeIncomingForBlock();//好像没找到实现对应功能的方法?
                    }else{
                        break;
                    }
                }
            }
            //需要清除指令和use链吗
            // for(auto i=bb->begin();i!=bb->end();++i){
                
            // }
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

//合并空返回块(no phi)
bool SimplifyCFG::mergeEmptyReturnBlocks(Function* func){
    auto& BBs=func->GetBBs();
    std::vector<BasicBlock*> returnBlocks;

    //收集所有空ret块
    for(auto& bbPtr:BBs){
        BasicBlock* bb=bbPtr.get();
        if(bb->Size()==1&&bb->front->id==Instruction::Op::Ret){
            returnBlocks.push_back(bb);
        }
    }
    //合并空ret块
    if(returnBlocks.size()<=1){
        return false;
    }
    //选定第一个作为公共返回块
    BasicBlock* commonRet=returnBlocks.front();

    //重定向其他返回块的前驱到commonRet,并清理phi
    for(size_t i=1;i<returnBlocks.size();++i){
        BasicBlock* redundant=returnBlocks[i];
        for(auto* pred: redundant->GetPredBlocks()){
            pred->ReplaceNextBlock(redundant,commonRet);//替换后继
            commonRet->AddPredBlock(pred);//加入新前驱
        }

        func->RemoveBBs(redundant);
    }
    return true;
}

//合并基本块(no phi)
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
    while(succ->Size()!=0){
        Instruction *inst=succ->GetFront();
        succ->erase(inst);
        bb->push_back(inst);
    }
    //更新CFG
    //断开bb与succ
    bb->RemoveNextBlock(succ);
    succ->RemovePredBlock(bb);
    //succ的后继接到bb上
    auto nexts=succ->GetNextBlocks();
    for(auto succsucc:nexts){
        succsucc->RemovePredBlock(succ);
        succsucc->AddPredBlock(bb);
        bb->AddNextBlock(succsucc);
    }
    succ->EraseFromManager();
    return true;
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
    ConstIRInt* c=dynamic_cast<ConstIRInt*>(cond);
    if(!c){
        return false;
    }
    BasicBlock* targetBlock=nullptr;
    if(c->isConstOne()){
        targetBlock=trueBlock;
    }else if(c->isConstZero()){
        targetBlock=falseBlock;
    }else{
        return false;
    }
    //创建无条件跳转指令,替换原条件跳转指令
    auto oldInst=bb->GetLastInsts();
    bb->erase(oldInst);
    Instruction* uncondBr=new UnCondInst(targetBlock);
    bb->push_back(uncondBr);

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