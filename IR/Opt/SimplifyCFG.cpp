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

    bool localChanged=false;
    do{
        localChanged=false;

        //简化条件跳转
        for(auto bb: func->GetBBs()){
            localChanged |= simplifyBranch(bb.get());
        }

        //合并线性块(无phi)
        for (auto bb : func->GetBBs()) {
            if (mergeBlocks(bb.get())) {
                localChanged = true;
                break; //一次合并后立刻跳出，避免使用已删除 successor
            }
        }
        // //合并跳转到唯一返回块
        // for (auto* bb : blocks) {
        //     localChanged |= mergeReturnJumps(bb);
        // }
        
        //清理CFG中的不可达基本块(否则phi会出错)
        // localChanged |= removeUnreachableBlocks();

        // 消除冗余 phi（统一值/已删除 predecessor）
        // for (auto bb : func->GetBBs()) {
        //     if(!bb) continue;
        //     localChanged |= eliminateTrivialPhi(bb.get());
        // }

        changed |= localChanged;
    }while(localChanged);//持续迭代直到收敛
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
// bool SimplifyCFG::removeUnreachableBlocks(){

//     bool changed=false;
//     std::unordered_set<BasicBlock*> reachable;
//     std::queue<BasicBlock*> worklist;

//     BasicBlock* entry = func->GetFront();
    
//     std::cerr << "[CFG-Debug] Entry block: " << entry->GetName() << "\n";
// for (auto& bb_ptr : func->GetBBs()) {
//     BasicBlock* bb = bb_ptr.get();
//     Instruction* term = bb->GetBack();
//     std::cerr << "  Block " << bb->GetName() << " terminator: ";
//     if (!term) std::cerr << "NONE!\n";
//     else std::cerr << term->OpToString(term->id) << " → ";
//     for (int i = 0; i < term->GetOperandNums(); ++i) {
//         auto* op = term->GetOperand(i);
//         if (auto* targetBB = dynamic_cast<BasicBlock*>(op)) {
//             std::cerr << targetBB->GetName() << " ";
//         } else {
//             std::cerr << "undef ";
//         }
//     }
//     std::cerr << "\n";
// }



//     if (!entry) {
//         std::cerr << "[removeUnreachableBlocks] entry is null!\n";
//         return false;
//     }
//     std::cerr << "entryblock is: " << entry->GetName() << std::endl;

//     reachable.insert(entry);
//     worklist.push(entry);

//     while (!worklist.empty()) {
//         BasicBlock* bb = worklist.front();
//         worklist.pop();
//         for (auto* succ : bb->GetNextBlocks()) {
//             if (reachable.insert(succ).second) {
//                 worklist.push(succ);
//             }
//         }
//     }

//     //打印下可达块看看
//     std::cerr << "[removeUnreachableBlocks] Reachable block list:\n";
//     for (auto* bb : reachable) {
//         std::cerr << "  " << bb->GetName() << "\n";
//     }

//     //修复所有跳转指令指向不可达块的情况
//     for(auto& bb_ptr:func->GetBBs()){
//         BasicBlock* bb=bb_ptr.get();

//         Instruction* term=bb->GetBack();
//         if(!term) continue;

//         bool updated = false;

//         for(int i=0;i<term->GetOperandNums();++i){
//             Value* op = term->GetOperand(i);
//             auto* target_bb = dynamic_cast<BasicBlock*>(op);

//             if (target_bb && !reachable.count(target_bb)){
//                 // 跳转目标不可达，替换成 UndefValue
//                 term->SetOperand(i, UndefValue::Get(target_bb->GetType()));
//                 updated=true;

//                 // 更新 CFG 维护前驱后继关系
//                 bb->RemoveNextBlock(target_bb);
//                 target_bb->RemovePredBlock(bb);

//                 std::cerr << "[removeUnreachableBlocks] Fixed jump in block " << bb->GetName()
//                           << " operand " << i << " from unreachable block " << target_bb->GetName() << "\n";
//             }
//         }

//         if(updated){
//             if(!term->IsTerminateInst()){
//                 std::cerr << "[removeUnreachableBlocks] Warning: block " << bb->GetName()
//                           << " terminator may be invalid after fix\n";
//             }
//         }
//     }

//     auto& BBList = func->GetBBs();
//     for(auto it = BBList.begin(); it != BBList.end(); ){
//         BasicBlock* bb=it->get();
//         if(bb==entry|| reachable.count(bb)){
//             ++it;
//             continue;
//         }

//         bool containsRet = false;
//         for (auto* inst : *bb) {
//             if (inst->id == Instruction::Op::Ret) {
//                 containsRet = true;
//                 break;
//             }
//         }

//         if (containsRet && !hasOtherRetInst( bb)) {
//             std::cerr << "[removeUnreachableBlocks] Skipping only return block: " << bb->GetName() << "\n";
//             ++it;
//             continue;
//         }

//         if (blockHasSideEffect(bb)) {
//             std::cerr << "[removeUnreachableBlocks] Side-effect block preserved: " << bb->GetName() << "\n";
//             ++it;
//             continue;
//         }

//         std::cerr << "[removeUnreachableBlocks] Removing unreachable block: " << bb->GetName() << "\n";

//         // 替换所有使用该块内产生的值
//         for (auto inst : *bb) {
//             inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
//         }

//         // 清理 phi 引用
//         for (auto* succ : bb->GetNextBlocks()) {
//             for (auto it2 = succ->begin(); it2 != succ->end(); ) {
//                 if (auto phi = dynamic_cast<PhiInst*>(*it2)) {
//                     phi->removeIncomingFrom(bb);
//                     ++it2;
//                 } else break;
//             }
//         }

//         // 更新 CFG 结构
//         for (auto* pred : bb->GetPredBlocks()) {
//             pred->RemoveNextBlock(bb);
//         }
//         for (auto* succ : bb->GetNextBlocks()) {
//             succ->RemovePredBlock(bb);
//         }

//         it = BBList.erase(it);  // 使用你的方式删除块
//         changed = true;
//     }
//     return changed;
// }

//合并空返回块(no phi)(实际上是合并所有返回相同常量值的返回块)
bool SimplifyCFG::mergeEmptyReturnBlocks(){
    auto& BBs=func->GetBBs();
    std::vector<BasicBlock*> ReturnBlocks;
    std::optional<int> commonRetVal;//optional用于标识一个值要么存在要么不存在(可选值)
    //记录目标常量返回值

    //收集所有返回指令,返回值需要是整数常量且值相同的块
    for(auto& bbPtr:BBs){
        BasicBlock* bb=bbPtr.get();
        //基本块内只有一条指令(ret)
        Instruction* lastInst=bb->GetLastInsts();
        if(!lastInst || lastInst->id!=Instruction::Op::Ret) continue;
        
        auto* retInst=dynamic_cast<RetInst*>(lastInst);
        if (!retInst || retInst->GetOperandNums() != 1) continue;

        Value* retVal=retInst->GetOperand(0);
        std::cerr << "Return block found: " << bb->GetName() << ", ret value: ";
        auto* c=dynamic_cast<ConstIRInt*>(retVal);
        if (c) std::cerr << c->GetVal() << "\n";
        else std::cerr << "non-const\n";

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
        std::cerr << (ReturnBlocks.empty() ? "No return blocks found.\n"
                                           : "Only one return block found, skipped merging.\n");
        return false;
    }

    std::cerr<<"Found"<<ReturnBlocks.size()<<" return blocks with common return value: "<<commonRetVal.value()<<"\n";

    //选定第一个作为公共返回块
    BasicBlock* commonRet=ReturnBlocks.front();

    bool changed=false;
    //重定向其他返回块的前驱到commonRet
    for(size_t i=1;i<ReturnBlocks.size();++i){
        BasicBlock* redundant=ReturnBlocks[i];
        // ⭐ Dominator 检查：如果 redundant 支配 commonRet，跳过，避免破坏控制流结构
        if (tree && tree->dominates(redundant, commonRet)) {
            std::cerr << "Skip merging " << redundant->GetName()
                      << " because it dominates common return block " << commonRet->GetName() << "\n";
            continue;
        }
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
        func->RemoveBBs(redundant);
        changed=true;
    }

    printAllTerminator(func, "after mergeERBlocks");
    return changed;
}

bool SimplifyCFG::simplifyBranch(BasicBlock* bb){
    if(!bb || bb->Size() == 0){
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
        // std::cerr << "[simplifyBranch] Not a constant condition in block: " << bb->GetName() << "\n";
        return false;
    }
    BasicBlock* targetBlock=c->GetVal() ? trueBlock:falseBlock;
    if(targetBlock==bb){
        // std::cerr << "[simplifyBranch] Target block is self-loop, skipped: " << bb->GetName() << "\n";
        return false;
    }

    //创建无条件跳转指令,替换原条件跳转指令
    bb->erase(lastInst);
    Instruction* uncondBr=new UnCondInst(targetBlock);
    bb->push_back(uncondBr);
    std::vector<BasicBlock*> toUpdateSuccs;
    if (targetBlock == trueBlock) {
        toUpdateSuccs = tree->getSuccBBs(falseBlock);  // falseBlock 将被删除
    } else {
        toUpdateSuccs = tree->getSuccBBs(trueBlock);   // trueBlock 将被删除
    }

    if (targetBlock == trueBlock){
        bb->NextBlocks = tree->getSuccBBs(bb);
        bb->RemoveNextBlock(falseBlock);

        for(auto* succBB:toUpdateSuccs){
            succBB->PredBlocks=tree->getPredBBs(succBB);
            succBB->RemovePredBlock(falseBlock);
        }
    }else{
        bb->NextBlocks = tree->getSuccBBs(bb);
        bb->RemoveNextBlock(trueBlock);

        for(auto* succBB:toUpdateSuccs){
            succBB->PredBlocks=tree->getPredBBs(succBB);
            succBB->RemovePredBlock(trueBlock);
        }
    }

    if (targetBlock == trueBlock) {
        func->RemoveBBs(falseBlock);
    } else {
        func->RemoveBBs(trueBlock);
    }

    tree->BuildDominantTree();

    BasicBlock* entry = func->GetFront();
    
    std::cerr << "[CFG-Debug-AfterSimplifyBranch] Entry block: " << entry->GetName() << "\n";
for (auto& bb_ptr : func->GetBBs()) {
    BasicBlock* bb = bb_ptr.get();
    Instruction* term = bb->GetBack();
    std::cerr << "  Block " << bb->GetName() << " terminator: ";
    if (!term) std::cerr << "NONE!\n";
    else std::cerr << term->OpToString(term->id) << " → ";
    for (int i = 0; i < term->GetOperandNums(); ++i) {
        auto* op = term->GetOperand(i);
        if (auto* targetBB = dynamic_cast<BasicBlock*>(op)) {
            std::cerr << targetBB->GetName() << " ";
        } else {
            std::cerr << "undef ";
        }
    }
    std::cerr << "\n";
}
    return true;
}

//合并基本块(no phi)
//不过只能合并线性路径,后面要补充
bool SimplifyCFG::mergeBlocks(BasicBlock* bb){
    //获取后继块
    bb->NextBlocks=tree->getSuccBBs(bb);
    if(bb->NextBlocks.size()!=1){
        return false;
    }
    auto succ=bb->NextBlocks[0];
    //后继不能是自身,避免死循环
    if(succ==bb){
        return false;
    }
    //判断succ是否只有bb一个前驱
    succ->PredBlocks=tree->getPredBBs(succ);
    if(succ->PredBlocks.size()!=1||succ->PredBlocks[0]!=bb){
        return false;
    }

    //不合并有副作用的块
    // if (blockHasSideEffect(bb) || blockHasSideEffect(succ)) return false;
    //不合并带phi的块
    for (auto it = succ->begin(); it != succ->end(); ++it) {
        if ((*it)->id == Instruction::Op::Phi) return false;
        break; // 只检查最前几条
    }
    //支配树判断：succ 是 bb 的 dominator（可能是 loop header）不安全
    if(tree&&tree->dominates(succ,bb)){
        return false;
    }

    //ok,那满足条件,合并
    //移除bb中的terminator指令(一般是br)
    if(bb->Size()!=0 && bb->GetBack()->IsTerminateInst()){
        bb->erase(bb->GetBack());
    }
    while (succ->Size() > 0) {
        Instruction *inst = succ->pop_front();
        inst->SetManager(bb);
        bb->push_back(inst);
    }

    //更新CFG
    auto nexts=tree->getSuccBBs(succ);
    for(auto succsucc:nexts){
        succsucc->PredBlocks=tree->getPredBBs(succsucc);
        // succsucc->RemovePredBlock(succ);
        succsucc->AddPredBlock(bb);
        bb->NextBlocks=tree->getSuccBBs(bb);
        bb->AddNextBlock(succsucc);
    }

    func->RemoveBBs(succ);
    DominantTree tree(func);
    tree.BuildDominantTree();


    BasicBlock* entry = func->GetFront();
    
    std::cerr << "[CFG-Debug-afterMergeBlocks] Entry block: " << entry->GetName() << "\n";
for (auto& bb_ptr : func->GetBBs()) {
    BasicBlock* bb = bb_ptr.get();
    Instruction* term = bb->GetBack();
    std::cerr << "  Block " << bb->GetName() << " terminator: ";
    if (!term) std::cerr << "NONE!\n";
    else std::cerr << term->OpToString(term->id) << " → ";
    for (int i = 0; i < term->GetOperandNums(); ++i) {
        auto* op = term->GetOperand(i);
        if (auto* targetBB = dynamic_cast<BasicBlock*>(op)) {
            std::cerr << targetBB->GetName() << " ";
        } else {
            std::cerr << "undef ";
        }
    }
    std::cerr << "\n";
}

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

    printAllTerminator(func, "after ePhi");

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