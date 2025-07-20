#include "../../include/IR/Opt/SimplifyCFG.hpp"

void SimplifyCFG::SafeEraseBlock(Function* func, BasicBlock* bb) {
    // 清除 CFG 边
    for (auto* succ : bb->GetNextBlocks()) {
        succ->RemovePredBlock(bb);
    }
    for (auto* pred : bb->GetPredBlocks()) {
        pred->RemoveNextBlock(bb);
    }

    // 清除 phi 引用
    for (auto* succ : bb->GetNextBlocks()) {
        for (auto it = succ->begin(); it != succ->end(); ++it) {
            if (auto* phi = dynamic_cast<PhiInst*>(*it)) {
                phi->removeIncomingFrom(bb);
            } else {
                break;
            }
        }
    }

    // 替换指令 use 为 undef
    for (auto it = bb->begin(); it != bb->end(); ++it) {
        Instruction* inst = *it;
        inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
    }

    // 从链表中移除
    func->erase(bb);

    // 再从 vector<shared_ptr<>> 中删
    auto& vec = func->GetBBs();
    vec.erase(std::remove_if(vec.begin(), vec.end(),
        [bb](const std::shared_ptr<BasicBlock>& ptr) { return ptr.get() == bb; }),
        vec.end());
}

bool SimplifyCFG::run() {
    return SimplifyCFGFunction(func);
}

//子优化顺序尝试
   bool SimplifyCFG::SimplifyCFGFunction(Function* func) {
    bool changed=false;
    int maxpass=10;

    do {
        changed = false;
        changed |= removeUnreachableBlocks(func);  // 先清理不可达块
        changed |= simplifyBranch(func);          // 简化分支
        changed |= eliminateTrivialPhi(func);     // 清理phi
        changed |= mergeBlocks(func);             // 合并块
        changed |= mergeEmptyReturnBlocks(func);  // 合并返回块


        if (--maxpass <= 0) {
            std::cerr << "WARN: Exceeded max optimization passes\n";
            break;
        }
    } while (changed);
    return changed;
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
        // SysY 可能的副作用函数
        static const std::unordered_set<std::string> sideEffectFuncs = {
            "putint", "putch", "putarray",
            "_sysy_starttime", "_sysy_stoptime",
            "getint", "getch", "getarray"
        };

        for (const auto& f : sideEffectFuncs) {
            if (callee == f) return true;
        }
        if(callee.find("_sysy")!=std::string::npos){
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
        Instruction* inst=b->GetLastInsts();
        if(!inst) continue;
        if (inst->IsTerminateInst() && inst->id==Instruction::Op::Ret){
            ++retCount;
        }
    }
    Instruction* inst=bb->GetLastInsts();
    if(!inst) return false;
    return retCount == 1 && inst->IsTerminateInst() && inst->id==Instruction::Op::Ret;
}
//新合并空返回块
bool SimplifyCFG::mergeEmptyReturnBlocks(Function* func){
    //1. 按返回值分组
    std::unordered_map<int,std::vector<BasicBlock*>> retValToBlocks;

    for (auto it = func->begin(); it != func->end(); ++it){
        BasicBlock* bb = *it;

        //检查条件:块必须只有一条指令且是ret
        if(bb->Size()!=1) continue;
        Instruction* lastInst=bb->GetBack();
        if(!lastInst || lastInst->id!=Instruction::Op::Ret) continue;

        //必须返回整数常量
        auto* retInst=dynamic_cast<RetInst*>(lastInst);
        if(!retInst || retInst->GetOperandNums()!=1) continue;
        //这样测试类型应该没问题了吧,待测试
        auto* c=dynamic_cast<ConstIRInt*>(retInst->GetUserUseList()[0]->GetValue());
        if(!c|| blockHasSideEffect(bb)) continue;
        //按返回值分组
        int val=c->GetVal();
        retValToBlocks[val].push_back(bb);
    } 
    //2. 合并每组相同返回值的块
    bool changed=false;
    for(auto& [val,blocks]:retValToBlocks){
        if(blocks.size()<2) continue;//至少两个块才需要合并

        BasicBlock* commonRet=blocks[0];//第一个块作为合并目标

        for(size_t i=1;i<blocks.size();++i){
            BasicBlock* redundant =blocks[i];

            //重定向前驱
            for(auto* pred:redundant->GetPredBlocks()){
                auto term=pred->GetBack();
                if (!term || !term->IsTerminateInst()) continue;

                //将跳转到redundant的指令改为跳转到commonRet
                for (int i = 0; i < term->GetOperandNums(); ++i){
                    if(term->GetOperand(i) == redundant){
                        term->SetOperand(i, commonRet);
                        pred->RemoveNextBlock(redundant);
                        pred->AddNextBlock(commonRet);
                        commonRet->AddPredBlock(pred);
                    }
                }
            }
            SafeEraseBlock(func, redundant);//从函数中删除块
            changed=true;
        }
    }
    return changed;
    // return true;
}
//新的删除不可达块
//list √
bool SimplifyCFG::removeUnreachableBlocks(Function* func) {
    bool changed=false;
    //1. DFS可能导致栈溢出,改为BFS
    std::unordered_set<BasicBlock*> reachable;
    std::queue<BasicBlock*> worklist;

    BasicBlock* entry=func->GetFront();
    if(!entry) return false;

    Instruction* term = entry->GetLastInsts();
    if (!term || !term->IsTerminateInst() || entry->GetNextBlocks().empty()){
        std::cerr << "[removeUnreachableBlocks] Skipped: entry block is not a valid CFG root.\n";
        return false;
    }
    reachable.insert(entry);
    worklist.push(entry);
    
    while (!worklist.empty()) {
        BasicBlock* bb = worklist.front();
        worklist.pop();
        
        // 遍历所有后继
        for (auto* succ : bb->GetNextBlocks()) {
            // 等价于:if (reachable.find(succ) == reachable.end()) { // 如果不存在
            //     reachable.insert(succ);                    // 插入
            //     worklist.push(succ);                       // 加入队列
            // }
            if (reachable.insert(succ).second) { // 如果新插入成功
                worklist.push(succ);
            }
        }
    }

    //2. 删除不可达块
    std::vector<BasicBlock*> toDelete;

    for (auto it = func->begin(); it != func->end(); ++it) {
        BasicBlock* bb = *it;
        if (bb == entry) continue;
        if (reachable.count(bb)) continue;

        if (isOnlyRetBlock(func, bb)) {
            std::cerr << "[removeUnreachableBlocks] Skipping only ret block: " << bb->GetName() << "\n";
            continue;
        }

        if (blockHasSideEffect(bb)) {
            std::cerr << "[removeUnreachableBlocks] Side-effect block unreachable but preserved: " << bb->GetName() << "\n";
            continue;
        }

        std::cerr << "[removeUnreachableBlocks] Mark unreachable: " << bb->GetName() << "\n";
        toDelete.push_back(bb);
    }

    for(auto* bb:toDelete){
        
        for(auto it=bb->begin();it!=bb->end();++it){
            Instruction* inst=*it;
            inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
        }
        
        //清理phi引用
        for(auto* succ:bb->GetNextBlocks()){
            for(auto it=succ->begin();it!=succ->end();++it){
                if(auto phi=dynamic_cast<PhiInst*>(*it)){
                    phi->removeIncomingFrom(bb);//移除来自该块的phi输入
                }else{
                    break;//phi只在块开头
                }
            }
        }

        //更新CFG
        for(auto* pred:bb->GetPredBlocks()){
            pred->RemoveNextBlock(bb);
        }
        for (auto* succ : bb->GetNextBlocks()) {
            succ->RemovePredBlock(bb);
        }
        SafeEraseBlock(func, bb);
        changed=true;
    }
    return changed;
}

//list √
bool SimplifyCFG::mergeBlocks(Function* func){
    bool changed = false;

    for (auto it = func->begin(); it != func->end(); ){
        BasicBlock* bb=*it;
        auto next_it=it;
        ++next_it;

        if (!bb || bb->Size() == 0) {
            it = next_it;
            continue;
        }

        //只有一个后继
        if(bb->NextBlocks.size()!=1){
            it=next_it;
            continue;
        }

        BasicBlock* succ=bb->NextBlocks[0];
        if(!succ||succ==bb){
            it==next_it;
            continue;
        }

        // 条件2：succ 只有一个前驱
        if (succ->PredBlocks.size() != 1 || succ->PredBlocks[0] != bb) {
            it = next_it;
            continue;
        }

        //succ 没有 phi 指令
        Instruction* firstInst=succ->GetFirstInsts();
        if(firstInst && dynamic_cast<PhiInst*>(firstInst)){
            it=next_it;
            continue;
        }

        //合并
        Instruction* term=bb->GetLastInsts();
        if(term){
            bb->erase(term);
        }

        while (succ->Size() > 0) {
            Instruction* inst = succ->GetFront();
            succ->erase(inst);
            bb->push_back(inst);
        }

        bb->RemoveNextBlock(succ);
        succ->RemovePredBlock(bb);

        for (auto succsucc : succ->NextBlocks) {
            bb->AddNextBlock(succsucc);
            succsucc->ReplacePreBlock(succ, bb);
        }

        succ->NextBlocks.clear();
        succ->PredBlocks.clear();

        SafeEraseBlock(func, succ);

        changed=true;
        it=func->begin();
    }
    return changed;
}

//vector
bool SimplifyCFG::simplifyBranch(Function* func){
    bool changed=false;
     std::vector<BasicBlock*> blocksToErase;
    for(auto* bb : *func){
        if (!bb || bb->Size() == 0) continue;

        auto* lastInst=*bb->rbegin();
        if(lastInst==*bb->rend()) continue;

        if(!lastInst||lastInst->id !=Instruction::Op::Cond) continue;

        auto* condInst=dynamic_cast<CondInst*>(lastInst);
        if(!condInst) continue;

        //获取条件操作数和两个基本块
        auto& uses = condInst->GetUserUseList();
        if (uses.size() < 3) continue;

        Value* cond = uses[0]->GetValue();
        Value* tVal = uses[1]->GetValue();
        Value* fVal = uses[2]->GetValue();

        auto* trueBlock = dynamic_cast<BasicBlock*>(tVal);
        auto* falseBlock = dynamic_cast<BasicBlock*>(fVal);

        if (!trueBlock || !falseBlock) continue;
        //两目标块相同
        if (trueBlock == falseBlock){
            Instruction* uncondBr=new UnCondInst(trueBlock);

            bb->erase(lastInst);
            bb->push_back(uncondBr);

            bb->RemoveNextBlock(trueBlock);
            bb->RemoveNextBlock(falseBlock);
            trueBlock->RemovePredBlock(bb);
            falseBlock->RemovePredBlock(bb);
            
            bb->AddNextBlock(trueBlock);
            trueBlock->AddPredBlock(bb);

            changed=true;
            continue;
        }

        //条件为常量bool
        auto* c=dynamic_cast<ConstIRBoolean*>(cond);
        if(c){
            BasicBlock* targetBlock=c->GetVal() ? trueBlock : falseBlock;
            BasicBlock* deadBlock = c->GetVal() ? falseBlock : trueBlock;

            Instruction* uncondBr=new UnCondInst(targetBlock);
            bb->erase(lastInst);
            bb->push_back(uncondBr);

            bb->RemoveNextBlock(trueBlock);
            bb->RemoveNextBlock(falseBlock);
            trueBlock->RemovePredBlock(bb);
            falseBlock->RemovePredBlock(bb);

            bb->AddNextBlock(targetBlock);
            targetBlock->AddPredBlock(bb);

            changed=true;
            continue;
        }
    }
    return changed;
}

//list √
bool SimplifyCFG::eliminateTrivialPhi(Function* func){
    bool changed=false;

    for(auto bbit=func->begin();bbit!=func->end();++bbit){
        BasicBlock* bb=*bbit;
        if(!bb||bb->Size()==0) continue;

        // 步骤1：收集当前块的所有phi指令
        std::list<PhiInst*> worklist;
        for (auto instIt = bb->begin(); instIt != bb->end(); ++instIt) {
            if (auto phi = dynamic_cast<PhiInst*>(*instIt)) {
                worklist.push_back(phi);
            } else {
                break; // phi只在块开头
            }
        }

        // 步骤2：处理每个phi指令
        for (auto phi : worklist){
            Value* sameVal = nullptr;
            bool allSame = true;

            // 检查所有输入值
            for (size_t i = 0; i < phi->GetOperandNums(); i += 2) {
                Value* val = phi->GetUserUseList()[i]->GetValue();
                if (dynamic_cast<UndefValue*>(val)) continue;

                if (!sameVal) {
                    sameVal = val;
                } else if (val != sameVal) {
                    allSame = false;
                    break;
                }
            }

            //1 所有实际输入相同
            if (allSame && sameVal) {
                phi->ReplaceAllUseWith(sameVal);
                bb->erase(phi); // 使用List的erase方法
                changed = true;
                continue;
            }
            //2 所有输入都是undef
            if (!sameVal) {
                auto undef = UndefValue::Get(phi->GetType());
                phi->ReplaceAllUseWith(undef);
                bb->erase(phi); // 使用List的erase方法
                changed = true;
            }
        }
    }
    return changed;
}