#include "../../include/IR/Opt/SimplifyCFG.hpp"

bool SimplifyCFG::run() {
    return SimplifyCFGFunction(func);
}

//子优化顺序尝试
//    bool SimplifyCFG::SimplifyCFGFunction(Function* func) {
//     bool changed=false;
//     int maxpass=10;

//     do {
//         // 每次创建新的block列表（防止迭代器失效）
//         auto blocks = func->GetBBs(); // 假设有这个方法
        
//         // 优化顺序调整（先处理简单变换）
//         changed |= simplifyBranch(func);       // 先处理条件跳转
//         changed |= mergeEmptyReturnBlocks(func); // 合并返回块
//         changed |= eliminateTrivialPhi(func);    // 清理phi
//         changed |= mergeBlocks(func);            // 合并块
//         changed |= removeUnreachableBlocks(func);// 最后删除死块

//         if (--maxpass <= 0) {
//             std::cerr << "WARN: Exceeded max optimization passes\n";
//             break;
//         }
//     } while (changed);
//     return changed;
//    }


bool SimplifyCFG::SimplifyCFGFunction(Function* func) {
    bool changed = false;

    // 1. 先合并常量返回块（不涉及 CFG）
    changed |= mergeEmptyReturnBlocks(func);
    int maxpass=5;
    bool localChanged = false;
    do {
        localChanged = false;

        // 构建块列表（每轮重新构建）
        std::vector<BasicBlock*> blocks;
        for (auto& bb_ptr : func->GetBBs()) {
            blocks.push_back(bb_ptr.get());
        }

        // 2. 简化 br i1 true/false
        for (auto* bb : blocks) {
            localChanged |= simplifyBranch(bb);
        }

        // 3. 清除死块（为 mergeBlocks 和 phi 提供干净 CFG）
        localChanged |= removeUnreachableBlocks(func);

        // 更新 block 列表
        blocks.clear();
        for (auto& bb_ptr : func->GetBBs()) {
            blocks.push_back(bb_ptr.get());
        }

        // 4. 合并线性块（前驱=1, 无 phi）
        for (auto* bb : blocks) {
            localChanged |= mergeBlocks(bb);
        }

        // 5. 清除无用 phi（只有一个值、来自死块的已清理）
        for (auto* bb : blocks) {
            localChanged |= eliminateTrivialPhi(bb);
        }

        changed |= localChanged;
    } while (localChanged && --maxpass>0);
}
// bool SimplifyCFG::SimplifyCFGBasicBlock(BasicBlock* bb){
//     bool changed=false;
//     changed |=simplifyBranch(bb);
//     changed |=mergeBlocks(bb);
//     changed |=eliminateTrivialPhi(bb);

//     return changed;
// }

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

    for(auto& bbptr:func->GetBBs()){
        BasicBlock* bb=bbptr.get();

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
            func->RemoveBBs(redundant);//从函数中删除块
            changed=true;
        }
    }
    return changed;
    // return true;
}
//合并空返回块(no phi)(实际上是合并所有返回相同常量值的返回块)
// bool SimplifyCFG::mergeEmptyReturnBlocks(Function* func){
//     auto& BBs=func->GetBBs();
//     std::vector<BasicBlock*> ReturnBlocks;
//     std::optional<int> commonRetVal;//optional用于标识一个值要么存在要么不存在(可选值)
//     //记录目标常量返回值

//     //收集所有返回指令,返回值需要是整数常量且值相同的块
//     for(auto& bbPtr:BBs){
//         BasicBlock* bb=bbPtr.get();
//         if(bb->Size()!=1) continue;
//         //基本块内只有一条指令(ret)
//         Instruction* lastInst=bb->GetLastInsts();
//         if(!lastInst || lastInst->id!=Instruction::Op::Ret) continue;
        
//         auto* retInst=dynamic_cast<RetInst*>(lastInst);
//         if (!retInst || retInst->GetOperandNums() != 1) continue;

//         Value* retVal=retInst->GetOperand(0);
//         auto* c=dynamic_cast<ConstIRInt*>(retVal);
//         if(!c) continue;
//         int val=c->GetVal();
//         if(!commonRetVal.has_value()){
//             std::cerr << "[mergeEmptyReturnBlocks] Return value mismatch, skipping block: " << bb->GetName() << "\n";
//             commonRetVal =val;
//         }
//         if(val!=commonRetVal.value()){
//             continue;
//         }

//         //跳过含副作用的块
//         if (blockHasSideEffect(bb)) {
//             std::cerr << "Skipped return block with side effects: " << bb->GetName() << "\n";
//             continue;
//         }

//         //跳过唯一合法ret块
//         if (isOnlyRetBlock(func,bb)) {
//             std::cerr << "Skipped only return block: " << bb->GetName() << "\n";
//             continue;
//         }

//         ReturnBlocks.push_back(bb);
//     }
//     //合并空ret块
//     if(ReturnBlocks.size()==1){
//         std::cerr << "Only one return block found: " << ReturnBlocks[0]->GetName() << ", skipped merging.\n";
//         return false;
//     }
//     if(ReturnBlocks.size()==0){
//         std::cerr << "No return blocks found.\n";
//         return false;
//     }
//     std::cerr<<"Found"<<ReturnBlocks.size()<<" return blocks with common return value: "<<commonRetVal.value()<<"\n";

//     //选定第一个作为公共返回块
//     BasicBlock* commonRet=ReturnBlocks.front();

//     bool changed=false;
//     //重定向其他返回块的前驱到commonRet
//     for(size_t i=1;i<ReturnBlocks.size();++i){
//         BasicBlock* redundant=ReturnBlocks[i];
//         std::cerr << "Removed redundant return block: " << redundant->GetName() << "\n";
//         //重定向所有前驱块的后继指针从redundant到commonRet
//         for(auto* pred: redundant->GetPredBlocks()){
//             if(pred->Size()==0) continue;
//             auto term=pred->GetLastInsts();
//             if(!term) continue;

//             bool replaced=false;
//             //替换terminator的operand
//             for(int i=0;i<term->GetOperandNums();++i){
//                 if(term->GetOperand(i)==redundant){
//                     term->SetOperand(i, commonRet);
//                     replaced=true;
//                     std::cerr << "    [Redirected] " << pred->GetName() << " -> " << commonRet->GetName() << "\n";
//                 }
//             }
//             if(replaced){
//                 pred->RemoveNextBlock(redundant);
//                 pred->AddNextBlock(commonRet);
//                 commonRet->AddPredBlock(pred);
//             }
//         }
//         //从函数中移除
//         std::cerr<< "Removed redundant return block: "<<redundant->GetName()<<"\n";
//         func->RemoveBBs(redundant);
//         changed=true;
//     }
//     return changed;
// }

//新的删除不可达块
bool SimplifyCFG::removeUnreachableBlocks(Function* func) {
    //1. DFS可能导致栈溢出,改为BFS
    std::unordered_set<BasicBlock*> reachable;
    std::queue<BasicBlock*> worklist;

    BasicBlock* entry=func->GetFront();
    if(!entry) return false;

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
    bool changed=false;
    std::vector<BasicBlock*> toDelete;

    for(auto& bbPtr:func->GetBBs()){
        BasicBlock* bb=bbPtr.get();
        if(reachable.count(bb)) continue;
        if(blockHasSideEffect(bb)) continue;
        toDelete.push_back(bb);
    }

    for(auto* bb:toDelete){
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
        func->RemoveBBs(bb);
        changed=true;
    }
    return changed;
}

// //删除不可达基本块(记得要把phi引用到的也进行处理)
// bool SimplifyCFG::removeUnreachableBlocks(Function* func){
//     std::unordered_set<BasicBlock*> reachable;//存储可达块
//     std::stack<BasicBlock*> bbstack;

//     auto entry=func->GetFront();
//     if(!entry) return false;
    
//     //如果入口块没有终结指令或没有后继,保守跳过清理
//     Instruction* term=entry->GetBack();
//     if(!term || !term->IsTerminateInst() || entry->GetNextBlocks().empty()){
//         std::cerr << "[removeUnreachableBlocks] Skipped: entry block is not a valid CFG root.\n";
//         return false;
//     }

//     reachable.insert(entry); 
//     bbstack.push(entry);
    

//     //DFS
//     while(!bbstack.empty()){
//         BasicBlock* bb=bbstack.top();
//         bbstack.pop();
//         for(auto& succ:bb->GetNextBlocks()){
//             if(reachable.insert(succ).second){
//                 bbstack.push(succ);
//             }
//         }
//     }

//     bool changed=false;
//     std::vector<BasicBlock*> toDelete;//准备删除列表
//     for(auto& bb:func->GetBBs()){
//         if(bb.get()==entry) continue;
//         if(reachable.count(bb.get())) continue;
//         //保护唯一返回块
//         if(isOnlyRetBlock(func,bb.get())){
//             std::cerr << "[removeUnreachableBlocks] Skipping only ret block: " << bb->GetName() << "\n";
//             continue;
//         }
//         //保守处理,副作用块即使不可达也不删
//         if (blockHasSideEffect(bb.get())) {
//             std::cerr << "[removeUnreachableBlocks] Side-effect block unreachable but preserved: " << bb->GetName() << "\n";
//             continue;
//         }
//         std::cerr << "[removeUnreachableBlocks] Mark unreachable: " << bb->GetName() << "\n";
//         //标记删除
//         toDelete.push_back(bb.get());
//     }


//     //遍历所有bb,移除不可达者
//     for (auto* bb : toDelete) {
//         std::cerr << "Erasing unreachable block: " << bb->GetName() << "\n";

//         // 替换所有值为 undef
//         for (auto* inst : *bb) {
//             inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
//         }

//         // 清除 phi 引用
//         for (auto* succ : bb->GetNextBlocks()) {
//             for (auto it = succ->begin(); it != succ->end(); ++it) {
//                 if (auto phi = dynamic_cast<PhiInst*>(*it)) {
//                     phi->removeIncomingFrom(bb);
//                 } else {
//                     break;
//                 }
//             }
//         }

//         // 清除 CFG 边
//         for (auto* pred : bb->GetPredBlocks()) {
//             pred->RemoveNextBlock(bb);
//         }
//         for (auto* succ : bb->GetNextBlocks()) {
//             succ->RemovePredBlock(bb);
//         }

//         // 从函数中删除
//         func->RemoveBBs(bb);
//         changed = true;
//     }
//     return changed;
// }


bool SimplifyCFG::simplifyBranch(BasicBlock* bb){
    if(!bb||bb->Size()==0){
        return false;
    }
    //获取基本块最后一条指令
    Instruction* lastInst=bb->GetBack();

    //判断是否条件跳转指令
    if (!lastInst || lastInst->id != Instruction::Op::Cond) {
        return false;
    }
    auto* condInst=dynamic_cast<CondInst*>(lastInst);
    if(!condInst){
        return false;
    }
    //获取条件操作数和两个基本块
    Value* cond=condInst->GetUserUseList()[0]->GetValue();
    BasicBlock* trueBlock=dynamic_cast<BasicBlock*>(condInst->GetUserUseList()[1]->GetValue());
    BasicBlock* falseBlock=dynamic_cast<BasicBlock*>(condInst->GetUserUseList()[2]->GetValue());
    //确认`目标基本块合法
    if(!trueBlock||!falseBlock){
        return false;
    }
    // 跳转块中是否有副作用？
    if (blockHasSideEffect(trueBlock) || blockHasSideEffect(falseBlock)) {
        std::cerr << "[simplifyBranch] Skip branch with side-effected targets: " << bb->GetName() << "\n";
        return false;
    }
    //判断条件是否是常量函数
    auto* c=dynamic_cast<ConstIRBoolean*>(cond);
    if(!c){
        std::cerr << "[simplifyBranch] Not a constant boolean condition in block: " << bb->GetName() << "\n";
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


    std::cerr << "[simplifyBranch] Simplified to: br label %" << targetBlock->GetName() << "\n";
    return true;
}

//新合并基本块
bool SimplifyCFG::mergeBlocks(BasicBlock* bb){
    //条件1 只有一个后继
    if(bb->GetNextBlocks().size()!=1) return false;

    BasicBlock* succ = bb->GetNextBlocks()[0];

    //条件2 不能合并到自身(防止循环)
    if(succ==bb) return false;

    //3 后继必须只有当前块一个前驱
    if (succ->GetPredBlocks().size() != 1 || succ->GetPredBlocks()[0] != bb)
        return false;
    //4 后继块不能有phi
    if(!succ->Size()==0 &&dynamic_cast<PhiInst*>(succ->front)){//这里判断条件没问题吧?
        return false;
    }    

    //步骤1 移除当前块的终止指令
    bb->pop_back();

    //2 将后继块指令移到当前块
    while(!succ->Size()==0){
        Instruction* inst=succ->pop_front();
        bb->push_back(inst);
    }

    //3 更新CFG
    //将succ的后继变为bb的后继
    for(auto succsucc:succ->GetNextBlocks()){
        succsucc->RemovePredBlock(succ);
        succsucc->AddPredBlock(bb);
        bb->AddNextBlock(succsucc);
    }

    //断开连接
    bb->RemoveNextBlock(succ);
    func->RemoveBBs(succ);

    return true;
}

//合并基本块(no phi)
//不过只能合并线性路径,后面要补充
// bool SimplifyCFG::mergeBlocks(BasicBlock* bb){
//     //获取后继块
//     if(bb->GetNextBlocks().size()!=1){
//         return false;
//     }
//     auto succ=bb->GetNextBlocks()[0];
//     //后继不能是自身,避免死循环
//     if(succ==bb){
//         return false;
//     }
//     //判断succ是否只有bb一个前驱
//     if(succ->GetPredBlocks().size()!=1||succ->GetPredBlocks()[0]!=bb){
//         return false;
//     }

//     //不合并含phi的块
//     if(!succ->Size()==0 && succ->front->id==Instruction::Op::Phi){
//         return false;
//     }
//     if(bb->Size()==0 || !bb->GetBack()->IsTerminateInst()){
//         return false;
//     }

//     //ok,那满足条件,合并
//     //移除bb中的terminator指令(一般是br)
//     bb->GetBack()->EraseFromManager();
//     while (succ->Size() > 0) {
//         Instruction *inst = succ->pop_front();
//         bb->push_back(inst);
//     }
//     //更新CFG

//     //succ的后继接到bb上
//     auto nexts=succ->GetNextBlocks();
//     for(auto succsucc:nexts){
//         succsucc->RemovePredBlock(succ);
//         succsucc->AddPredBlock(bb);
//         bb->AddNextBlock(succsucc);
//     }

//     //断开bb与succ
//     bb->RemoveNextBlock(succ);
//     succ->RemovePredBlock(bb);

//     func->RemoveBBs(succ);
//     return true;
// }

//消除无意义phi
// bool SimplifyCFG::eliminateTrivialPhi(BasicBlock* bb){
//     bool changed=false;

//     //遍历当前基本块中所有指令
//     for(auto it=bb->begin();it!=bb->end();){
//         Instruction* inst=*it;

//         //isphi?
//         if(inst->id==Instruction::Op::Phi){
            
//             //是否只有一个输入来源块
//             std::unordered_set<BasicBlock*> incomingBlocks;
//             for(size_t i=0;i<inst->GetOperandNums();i+=2){
//                 if(auto* pred = dynamic_cast<BasicBlock*>(inst->GetOperand(i+1))){
//                     incomingBlocks.insert(pred);
//                 }
//             }

//             //主干phi,来源块>=2
//             if(incomingBlocks.size()>=2 && blockHasSideEffect(bb)){
//                 std::cerr << "[eliminateTrivialPhi] Skip phi in block with side effects: " << bb->GetName() << "\n";
//                 ++it;
//                 continue;
//             }
//             //输入只有一个来源,也可以删掉
//             if(inst->GetOperandNums()==2){
//                 inst->ReplaceAllUseWith(inst->GetOperand(0));
//                 auto to_erase=it;
//                 ++it;
//                 bb->erase(*to_erase);
//                 changed=true;
//                 continue;
//             }
            
//             bool has_undef = false;
//             Value* same=nullptr;
//             bool all_same=true;

//             //遍历所有phi的输入值
//             for(size_t i=0;i<inst->GetOperandNums();i+=2){
//                 Value* val=inst->GetOperand(i);
//                 if (dynamic_cast<UndefValue*>(val)) {
//                     has_undef = true;
//                     continue;
//                 }
//                 if(!same){
//                     same=val;
//                 }else if(val!=same){
//                     all_same=false;
//                     break;
//                 }
//             }

//             //所有输入值相同,可以替换   
//             if(same==nullptr){
//                 //所有输入都是undef或self,替换为undef
//                 auto undef=UndefValue::Get(inst->GetType());
//                 inst->ReplaceAllUseWith(undef);
//                 auto to_erase=it;
//                 ++it;
//                 bb->erase(*to_erase);
//                 changed=true;
//                 continue;
//             }
//             if(all_same){
//                 inst->ReplaceAllUseWith(same);
                
//                 auto to_erase=it;
//                 ++it;
//                 bb->erase(*to_erase);
                
//                 changed=true;
//                 continue;
//             }
//         }
//         ++it;
//     }
//     return changed;
// }

//新消除无意义phi
bool SimplifyCFG::eliminateTrivialPhi(BasicBlock* bb){
    bool changed = false;
    std::vector<PhiInst*> worklist;

    //步骤1 收集所有phi指令
    for(auto it=bb->begin();it!=bb->end();++it){
        if(auto phi=dynamic_cast<PhiInst*>(*it)){
            worklist.push_back(phi);
        }else{
            break;
        }
    }

    //2 处理每个phi
    for(auto phi:worklist){
        Value* sameVal = nullptr;
        bool allSame = true;

        // 检查所有输入值
        for(size_t i = 0; i < phi->GetOperandNums(); i += 2){
            Value* val = phi->GetUserUseList()[i]->GetValue();
            if (dynamic_cast<UndefValue*>(val)) continue;

            if(!sameVal){
                sameVal=val;
            }else if(val!=sameVal){
                allSame=false;
                break;
            }
        }

        //情况1 所有实际输入相同
        if(allSame && sameVal){
            phi->ReplaceAllUseWith(sameVal);
            bb->erase(phi);
            changed =true;
        }
        //2 所有输入都是undef
        else if(!sameVal){
            auto undef =UndefValue::Get(phi->GetType());
            phi->ReplaceAllUseWith(undef);
            bb->erase(phi);
            changed=true;
        }
    }
    return changed;
}