#include "../../include/IR/Opt/SimplifyCFG.hpp"

void printAllTerminator(Function* func, const std::string& tag = "") {
    std::cerr << "[TerminatorCheck] " << tag << " ====================\n";
    for (auto& bb_ptr : func->GetBBs()) {
        BasicBlock* bb = bb_ptr.get();
        Instruction* term = bb->GetBack();
        std::cerr << "Block " << bb->GetName() << ": ";
        if (!term) {
            std::cerr << "NO terminator!\n";
        } else {
            std::cerr << "Terminator: " << term->OpToString(term->id) << "\n";
        }
    }
}

bool SimplifyCFG::run() {
    return SimplifyCFGFunction();
}

//子优化顺序尝试
bool SimplifyCFG::SimplifyCFGFunction(){
    bool changed=false;

    //1. 尝试合并空返回块
    // changed |= mergeEmptyReturnBlocks();
    const int max_iter = 10;
    int iter=0;
    bool localChanged=false;
    do{

        if (++iter > max_iter) {
            break;
        }

        localChanged=false;

        //简化条件跳转
        localChanged |=simplifyBranch();
        

        //合并线性块(无phi)
        for (auto bb : func->GetBBs()) {
            if (mergeBlocks(bb.get())) {
                localChanged = true;
                break; //一次合并后立刻跳出，避免使用已删除 successor
            }
        }

        // 消除冗余 phi（统一值/已删除 predecessor）
        // for (auto bb : func->GetBBs()) {
        //     if(!bb) continue;
        //     localChanged |= eliminateTrivialPhi(bb.get());
        // }

        changed |= localChanged;
    }while(localChanged);//持续迭代直到收敛

    DominantTree _tree(func);
    _tree.BuildDominantTree();
    return true;
}
//暂时没考虑无返回值情况
bool SimplifyCFG::hasOtherRetInst(BasicBlock* bb_){
    for(auto& bb_ptr:func->GetBBs()){
        BasicBlock* bb=bb_ptr.get();
        if(bb==bb_) continue;
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

        if (sideEffectFuncs.count(callee) > 0 || callee.find("_sysy") != std::string::npos)
            return true;

        if(inst->id==Instruction::Op::Store) {
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

// //删除不可达基本块(记得要把phi引用到的也进行处理)
bool SimplifyCFG::removeUnreachableBlocks(){

    bool changed=false;
    std::unordered_set<BasicBlock*> reachable;
    std::queue<BasicBlock*> worklist;

    BasicBlock* entry = func->GetFront();
    if (!entry) {
        return false;
    }
    reachable.insert(entry);
    worklist.push(entry);
    while (!worklist.empty()) {
        BasicBlock* bb = worklist.front();
        worklist.pop();
        // bb->NextBlocks=_tree.getSuccBBs(bb);
        for (auto* succ : bb->GetNextBlocks()) {
            if (!succ) continue;
            if (reachable.insert(succ).second) {
                worklist.push(succ);
            }
        }
    }

    std::vector<BasicBlock*> unreachable_blocks;
    for (auto& bb_ptr : func->GetBBs()) {
        BasicBlock* bb = bb_ptr.get();
        if (!bb || bb == entry || reachable.count(bb)) continue;
        unreachable_blocks.push_back(bb);
    }
    for(auto* bb : unreachable_blocks ){

        // 清理 phi 引用
        for (auto* succ : bb->GetNextBlocks()) {
            if (!succ) continue;
            for (auto it2 = succ->begin(); it2 != succ->end(); ) {
                if (auto phi = dynamic_cast<PhiInst*>(*it2)) {
                    phi->removeIncomingFrom(bb);
                    ++it2;
                } else break;
            }
        }

        // 更新 CFG 结构
        // bb->PredBlocks=_tree.getPredBBs(bb);
        for (auto* pred : bb->GetPredBlocks()) {
            if(!pred) continue;
            // pred->NextBlocks=_tree.getSuccBBs(pred);
            pred->RemoveNextBlock(bb);
            bb->RemovePredBlock(pred);
        }
        
        for (auto* succ : bb->GetNextBlocks()) {
            if(!succ) continue;
            // succ->PredBlocks=_tree.getPredBBs(succ);
            succ->RemovePredBlock(bb);
            bb->RemoveNextBlock(succ);
        }

        func->RemoveBBs(bb);
        changed = true;
    }
    //add 删掉处理之后剩的孤立块
    auto& BBList = func->GetBBs();
    for (auto it = BBList.begin(); it != BBList.end(); ) {
        BasicBlock* bb = it->get();
        if (!bb || bb == entry) {
            ++it;
            continue;
        }

        if (bb->GetPredBlocks().empty()) {
            std::cerr << "[removeUnreachableBlocks] Orphan block removed: " << bb->GetName() << "\n";

            for (auto* succ : bb->GetNextBlocks()) {
                if (succ) succ->RemovePredBlock(bb);
            }

            func->RemoveBBs(bb);
            it = BBList.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    return changed;
}

//合并空返回块(no phi)(实际上是合并所有返回相同常量值的返回块)
bool SimplifyCFG::mergeEmptyReturnBlocks(){
    DominantTree _tree(func);
    _tree.BuildDominantTree();

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
        // std::cerr << "Return block found: " << bb->GetName() << ", ret value: ";
        auto* c=dynamic_cast<ConstIRInt*>(retVal);
        if(!c) continue;
        int val=c->GetVal();
        if(!commonRetVal.has_value()){
            commonRetVal =val;
        }
        if(val==commonRetVal.value()){
            ReturnBlocks.push_back(bb);
        }
    }
    //合并空ret块
    if (ReturnBlocks.size() <= 1) {
        // std::cerr << "No need to merge return blocks.\n";
        return false;
    }

    // std::cerr<<"Found"<<ReturnBlocks.size()<<" return blocks with common return value: "<<commonRetVal.value()<<"\n";

    //选定第一个作为公共返回块
    BasicBlock* commonRet=ReturnBlocks.front();
    commonRet->PredBlocks=_tree.getPredBBs(commonRet);
    // commonRet->PredBlocks.clear();//会有问题吗提前空,我觉得应该没问题,返回块
    bool changed=false;

    //重定向其他返回块的前驱到commonRet
    for(size_t i=1;i<ReturnBlocks.size();++i){
        BasicBlock* redundant=ReturnBlocks[i];
        //Dominator 检查：如果 redundant 支配 commonRet，跳过，避免破坏控制流结构
        if (_tree.dominates(redundant, commonRet)) {
            // std::cerr << "Skip merging " << redundant->GetName()
            //           << " because it dominates common return block " << commonRet->GetName() << "\n";
            continue;
        }
        //重定向所有前驱块的后继指针从redundant到commonRet
        std::vector<BasicBlock*> preds = _tree.getPredBBs(redundant);
        for(auto* pred: preds){
            auto term=pred->GetLastInsts();
            if(!term) continue;

            bool replaced=false;
            // 替换terminator的operand
            for(int i=0;i<term->GetOperandNums();++i){
                if(term->GetOperand(i)==redundant){
                    term->SetOperand(i, commonRet);
                    //应该是这样?
                    // Use* u = term->GetUserUseList()[i].get();
                    // Value* oldVal = u->usee;
                    // oldVal->GetValUseList().remove(u);
                    // u->usee = commonRet;
                    // commonRet->GetValUseList().push_front(u);


                    replaced=true;
                    // std::cerr << "    [Redirected] " << pred->GetName() << " -> " << commonRet->GetName() << "\n";
                }
            }
            if(replaced){
                pred->NextBlocks=_tree.getSuccBBs(pred);
                pred->RemoveNextBlock(redundant);
                pred->AddNextBlock(commonRet);
                commonRet->AddPredBlock(pred);
            }
        }
        redundant->PredBlocks=_tree.getPredBBs(redundant);
        redundant->PredBlocks.clear();
        
        // // 那个ret i32 0的0是是ConstIRInt类型,是全局Value,所以在删除redundant时要遍历指令清空其User的所有use?
        // for (auto inst = redundant->begin(); inst != redundant->end(); ++inst) {
        //     (*inst)->DropAllUsesOfThis();  // 清空这个指令用到的 Value 的 use 链
        // }
        //从函数中移除
        func->RemoveBBs(redundant);
        changed=true;
    }

    
    return changed;
}

bool SimplifyCFG::simplifyBranch(){

    DominantTree _tree(func);
    _tree.BuildDominantTree();
    for (auto& bb_ptr : func->GetBBs()) {
        BasicBlock* B = bb_ptr.get();
        if (!B) continue;
        B->PredBlocks = _tree.getPredBBs(B);
        B->NextBlocks = _tree.getSuccBBs(B);
    }

    bool changed=false;

    for(auto bb_ptr: func->GetBBs()){
        BasicBlock* bb = bb_ptr.get();
        if(!bb || bb->Size() == 0){
            continue;
        }
        //获取基本块最后一条指令
        Instruction* lastInst=bb->GetBack();

        //判断是否条件跳转指令
        bool is_cond_branch = lastInst && lastInst->id==Instruction::Op::Cond;
        if(!is_cond_branch){
            continue;
        }
        //获取条件操作数和两个基本块
        Value* cond=lastInst->GetOperand(0);
        BasicBlock* trueBlock=dynamic_cast<BasicBlock*>(lastInst->GetOperand(1));
        BasicBlock* falseBlock=dynamic_cast<BasicBlock*>(lastInst->GetOperand(2));
        //确认`目标基本块合法
        if(!trueBlock||!falseBlock){
            continue;
        }
        //判断条件是否是常量函数
        auto* c=dynamic_cast<ConstIRBoolean*>(cond);
        if(!c){
            // std::cerr << "[simplifyBranch] Not a constant condition in block: " << bb->GetName() << "\n";
            continue;
        }
        BasicBlock* targetBlock=c->GetVal() ? trueBlock:falseBlock;
        BasicBlock* deadBlock = (targetBlock == trueBlock) ? falseBlock : trueBlock;
        if(targetBlock==bb){
            // std::cerr << "[simplifyBranch] Target block is self-loop, skipped: " << bb->GetName() << "\n";
            continue;
        }

        bb->erase(lastInst);
        Instruction* uncondBr = new UnCondInst(targetBlock);
        bb->push_back(uncondBr); 

        deadBlock->RemovePredBlock(bb);
        bb->RemoveNextBlock(deadBlock);
        changed=true;
    }
    removeUnreachableBlocks();
    return changed;
}

//合并基本块(no phi)
//不过只能合并线性路径,后面要补充
bool SimplifyCFG::mergeBlocks(BasicBlock* bb){
    
    DominantTree _tree(func);
    _tree.BuildDominantTree();
    
    //获取后继块
    bb->NextBlocks=_tree.getSuccBBs(bb);
    if(bb->NextBlocks.size()!=1){//避免succ空
        return false;
    }
    auto succ=bb->NextBlocks[0];
    //后继不能是自身,避免死循环
    if(!succ||succ==bb){
        return false;
    }
    //后继不能是终结块
    if(succ->Size()>=0 && succ->GetBack()&& succ->GetBack()->id==Instruction::Op::Ret){
        return false;
    }
    //判断succ是否只有bb一个前驱
    succ->PredBlocks=_tree.getPredBBs(succ);
    if(succ->PredBlocks.size()!=1||succ->PredBlocks[0]!=bb){
        return false;
    }


    //支配树判断：succ 是 bb 的 dominator（可能是 loop header）不安全
    if(_tree.dominates(succ,bb)){
        return false;
    }
    //不合并带phi的块
    for (auto it = succ->begin(); it != succ->end(); ++it) {
        if((*it)==nullptr) return false;
        if ((*it)->id == Instruction::Op::Phi) return false;
        break; // 只检查最前几条
    }

    std::vector<BasicBlock*> succsuccs = _tree.getSuccBBs(succ);
    for (auto* succsucc : succsuccs){
        if (!succsucc) continue;//避免succsucc是空的
        for (auto it = succsucc->begin(); it != succsucc->end(); ++it){
            if (!(*it)) continue; 
            if ((*it)->id != Instruction::Op::Phi) break;
            auto* phi=dynamic_cast<PhiInst*>(*it);
            if (!phi) continue;
            int num = phi->getNumIncomingValues();
            for(int i=0;i<num;++i){
                BasicBlock* label=phi->getIncomingBlock(i);
                if (!label) continue;
                if(label==bb|| label == succ) return false;
            }
        }
    }

    //ok,那满足条件,合并
    //移除bb中的terminator指令(一般是br)
    if(bb->Size()!=0 && bb->GetBack() && bb->GetBack()->IsTerminateInst()){
        bb->erase(bb->GetBack());
    }
    while (Instruction* inst = succ->pop_front()) {
        inst->SetManager(bb);
        bb->push_back(inst);
    }

    //更新CFG
    bb->NextBlocks=_tree.getSuccBBs(bb);
    bb->RemoveNextBlock(succ);
    succ->NextBlocks=_tree.getSuccBBs(succ);
    std::vector<BasicBlock*> toUpdateSuccs = _tree.getSuccBBs(succ);
    for(auto succsucc:toUpdateSuccs){
        if (!succsucc) continue;
        succsucc->PredBlocks=_tree.getPredBBs(succsucc);
        succsucc->RemovePredBlock(succ);
        succ->RemoveNextBlock(succsucc);
        succsucc->AddPredBlock(bb);
        bb->AddNextBlock(succsucc);
    }

    func->RemoveBBs(succ);

    return true;
}

//消除无意义phi
bool SimplifyCFG::eliminateTrivialPhi(BasicBlock* bb){
    DominantTree _tree(func);
    _tree.BuildDominantTree();
    
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

// bool SimplifyCFG::mergeReturnJumps(BasicBlock* bb){
//     if (!bb || bb->Size() == 0) return false;
//     // 获取最后一条指令：必须是无条件跳转
//     Instruction* term = bb->GetBack();
//     if (!term || term->id != Instruction::Op::UnCond) return false;

//     BasicBlock* target = dynamic_cast<BasicBlock*>(term->GetUserUseList()[0]->GetValue());
//     if (!target || target == bb) return false;


// }