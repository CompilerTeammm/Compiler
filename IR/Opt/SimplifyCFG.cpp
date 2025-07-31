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
    const int max_iter = 15;
    int iter=0;
    bool localChanged=false;
    do{

        if (++iter > max_iter) {
            break;
        }

        localChanged=false;

        //简化条件跳转
        localChanged |=simplifyBranch();
        
        localChanged |= CleanPhi();


        //合并线性块(无phi)
        for (auto bb : func->GetBBs()) {
            if (mergeBlocks(bb.get())) {
                localChanged = true;
                break; //一次合并后立刻跳出，避免使用已删除 successor
            }
        }
        changed |= localChanged;
    }while(localChanged);//持续迭代直到收敛
    
    
    changed |=CleanPhi();//最后对于phi的合法性做个保障


    DominantTree _tree(func);
    _tree.BuildDominantTree();
    return true;
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

        for (auto* inst : *bb) {
            inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
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
    // if(succ->Size()>=0 && succ->GetBack()&& succ->GetBack()->id==Instruction::Op::Ret){
    //     return false;
    // }
    //判断succ是否只有bb一个前驱
    succ->PredBlocks=_tree.getPredBBs(succ);
    if(succ->PredBlocks.size()!=1||succ->PredBlocks[0]!=bb){
        return false;
    }


    //支配树判断：succ 是 bb 的 dominator（可能是 loop header）不安全
    if(_tree.dominates(succ,bb)){
        bool isLoopHeader=false;
        for(auto* succsucc : _tree.getSuccBBs(succ)){
            if (succsucc == bb){
                isLoopHeader = true;
                break;
            }
        }
        if (isLoopHeader){
            return false;
        }
        // 否则是线性路径支配，允许
    }
    //不合并带phi的块,只合并唯一incoming的phi
    bool onlyTrivialPhi = true;
    for (auto it = succ->begin(); it != succ->end(); ++it) {
        if (!(*it)) continue;
        if ((*it)->id != Instruction::Op::Phi) break;
        auto* phi = dynamic_cast<PhiInst*>(*it);
        if (!phi) {
            onlyTrivialPhi = false;
            break;
        }
        if (phi->getNumIncomingValues() != 1) {
            onlyTrivialPhi = false;
            break;
        }
    }
    if (!onlyTrivialPhi)
        return false;

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

    for (auto it = succ->begin(); it != succ->end();) {
        Instruction* inst = *it;
        ++it;
    
        if (inst->id == Instruction::Op::Phi) {
            auto* phi = dynamic_cast<PhiInst*>(inst);
            if (phi && phi->getNumIncomingValues() == 1) {
                // 获取唯一 incoming 的 value
                Value* incoming_val = phi->getIncomingValue(0);
                Value* target = phi;  // 该 phi 的定义
    
                // 替换所有 use
                target->ReplaceAllUseWith(incoming_val);
    
                // 删除 phi 指令
                succ->erase(inst);
            }
        }
    }
    //ok,那满足条件,合并
    //移除bb中的terminator指令(一般是br)
    if(bb->Size()!=0 && bb->GetBack() && bb->GetBack()->IsTerminateInst()){
        bb->erase(bb->GetBack());
    }
    //在这里将替换所有phi指令中incomingblock=succ的条目为bb
    for(auto& block : func->GetBBs()){
        if (!block) continue;
        for (auto it = block->begin(); it != block->end(); ++it){
            if (!(*it) || (*it)->id != Instruction::Op::Phi) break;
            auto* phi = dynamic_cast<PhiInst*>(*it);
            if (!phi) continue;
            int num = phi->getNumIncomingValues();
            for (int i = 0; i < num; ++i){
                if (phi->getIncomingBlock(i) == succ){
                    phi->SetIncomingBlock(i,bb);
                }
            }
        }
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
bool SimplifyCFG::CleanPhi(){
    bool changed = false;

    DominantTree _tree(func);
    _tree.BuildDominantTree();

    for (auto& block : func->GetBBs()){
        if (!block) continue;
        block->PredBlocks=_tree.getPredBBs(block.get());

        // 获取 block 的前驱集合
        std::set<BasicBlock*> predSet(block->PredBlocks.begin(), block->PredBlocks.end());

        for (auto it = block->begin(); it != block->end(); ){
            Instruction* inst = *it;
            if (!inst || inst->id != Instruction::Op::Phi) break;

            auto* phi = dynamic_cast<PhiInst*>(inst);
            if (!phi) {
                ++it;
                continue;
            }
            // 收集不合法的 incoming
            std::vector<BasicBlock*> toRemove;
            for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
                BasicBlock* from = phi->getIncomingBlock(i);
                if (!predSet.count(from)) {
                    toRemove.push_back(from);
                }
            }

            if (!toRemove.empty()) {
                for (auto* badFrom : toRemove) {
                    phi->removeIncomingFrom(badFrom);
                }
                changed = true;
            }

            if (phi->getNumIncomingValues() == 1) {
                Value* val = phi->getIncomingValue(0);
                phi->ReplaceAllUseWith(val);
                Instruction* inst = *it;
                ++it;  // 先递增，确保安全
                block->erase(inst);  // 删除指令
                changed = true;
                continue;
            }

            ++it;
        }
    }
    return changed;
}