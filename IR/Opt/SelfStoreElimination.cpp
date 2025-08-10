#include "../../include/IR/Opt/SelfStoreElimination.hpp"

bool SelfStoreElimination::run() {
    DFSOrder.clear();
    wait_del.clear();
    func->init_visited_block();
    //与DSE一样,构造DFS逆后序
    OrderBlock(func->front);
    std::reverse(DFSOrder.begin(), DFSOrder.end());

    // 如果整个函数有副作用，则跳过优化，保守处理
    // if (sideEffect && sideEffect->FuncHasSideEffect(func)) {
    //     std::cerr << "[SSE] skipped due to side effect: " << func->GetName() << "\n";
    //     return false;
    // }

    std::unordered_map<Value*, std::vector<User*>> storeMap;
    CollectStoreInfo(storeMap); // 收集“每个地址”上的所有 store
    CheckSelfStore(storeMap); // 保守判断冗余并插入待删列表

    removeInsts();

    return !wait_del.empty();
}

void SelfStoreElimination::OrderBlock(BasicBlock* bb) {
    if (bb->visited) return;
    bb->visited = true;

    // 获取该基本块的支配树节点
    auto* node = tree->getNode(bb);
    for (auto succNode : node->succNodes) {
        OrderBlock(succNode->curBlock);
    }

    DFSOrder.push_back(bb);
}

void SelfStoreElimination::CollectStoreInfo(std::unordered_map<Value*, std::vector<User*>>& storeMap) {
    for(auto* bb:DFSOrder){
        for(auto* inst:*bb){
            if (auto store = dynamic_cast<StoreInst*>(inst)) {
                Value* dst = store->GetOperand(1); // 存储地址

                //GEP->ALLOCA
                if(auto* gep=dynamic_cast<GepInst*>(dst)){
                    Value* gep_base = gep->GetOperand(0);
                    if (auto* alloca = dynamic_cast<AllocaInst*>(gep_base)) {
                        storeMap[alloca].push_back(store);
                    }
                }else if(auto* alloca = dynamic_cast<AllocaInst*>(dst)){
                    storeMap[alloca].push_back(store);
                }else if(!dst->isGlobal()){
                    storeMap[dst].push_back(store);
                }
            }
        }
    }
    for (auto& [val, list] : storeMap) {
        std::cerr << "[Collect] Store target: " << val->GetName() << ", Count: " << list.size() << "\n";
    }
}

static bool IsSameGEP(Value* a, Value* b) {
    auto* gep1 = dynamic_cast<GepInst*>(a);
    auto* gep2 = dynamic_cast<GepInst*>(b);
    if (!gep1 || !gep2) return false;

    if (gep1->GetOperandNums() != gep2->GetOperandNums())
        return false;

    for (int i = 0; i < gep1->GetOperandNums(); ++i) {
        if (gep1->GetOperand(i) != gep2->GetOperand(i))
            return false;
    }
    return true;
}



void SelfStoreElimination::CheckSelfStore(std::unordered_map<Value*, std::vector<User*>>& storeMap) {
    std::set<Value*> unsafe_addr;

    for (auto* bb : DFSOrder) {
        for (auto* inst : *bb) {
            // case 1: 自写入 store(load(x), x)
            if (auto* store = dynamic_cast<StoreInst*>(inst)) {
                Value* src = store->GetOperand(0);
                Value* dst = store->GetOperand(1);

                if (auto* load = dynamic_cast<LoadInst*>(src)) {
                    Value* load_src = load->GetOperand(0);
                    if (load_src == dst || IsSameGEP(load_src, dst)) {
                        if (!wait_del.count(store)){
                            wait_del.insert(store);  // 直接删除该指令，不清除 map
                        }
                        
                    }
                }
            }

            // case 2: GEP 被其他非 store / GEP 使用
            else if (auto* gep = dynamic_cast<GepInst*>(inst)) {
                Value* base = gep->GetOperand(0);
                if (storeMap.count(base)) {
                    for (auto* use : gep->GetValUseList()) {
                        User* user = use->GetUser();
                        if (!dynamic_cast<StoreInst*>(user) && !dynamic_cast<GepInst*>(user)) {
                            unsafe_addr.insert(base); // 标记该地址不安全
                            break;
                        }
                    }
                }
            }

            // case 3: 调用了无法分析的函数，保守跳过所有
            else if (auto* call = dynamic_cast<CallInst*>(inst)) {
                std::string name = call->GetOperand(0)->GetName();
                if (name != "llvm.memcpy.p0.p0.i32") {
                    // 其他函数调用不可分析
                    storeMap.clear();
                    return;
                }

                // memcpy 处理
                Value* dst = call->GetOperand(1);
                if (auto* gep = dynamic_cast<GepInst*>(dst)) {
                    if (auto* alloca = dynamic_cast<AllocaInst*>(gep->GetOperand(0))) {
                        storeMap[alloca].push_back(call);
                    }
                } else if (auto* alloca = dynamic_cast<AllocaInst*>(dst)) {
                    storeMap[alloca].push_back(call);
                }
            }

            // case 4: 普通指令使用到了 alloca，不再安全
            else {
                for (auto& use : inst->GetUserUseList()) {
                    Value* v = use->usee;
                    unsafe_addr.insert(v);
                }
            }
        }
    }

    // 删除所有 unsafe 地址
    for (Value* addr : unsafe_addr) {
        storeMap.erase(addr);
    }

    // case 5: 对剩余 safe 地址，保留最后一次写，前面的都冗余
    for (auto& [addr, stores] : storeMap) {
        for (size_t i = 0; i + 1 < stores.size(); ++i) {
            if (!wait_del.count(stores[i])){
                wait_del.insert(stores[i]);
            }
            
        }
    }
}

void SelfStoreElimination::removeInsts() {
    for (auto* user : wait_del) {
        if (auto* inst = dynamic_cast<Instruction*>(user)) {
            if (!inst->GetParent()) {
                std::cerr << "[SSE] Warning: Attempt to remove already-erased instruction: "
                          << inst->GetName() << "\n";
                continue;
            }
            
            inst->ClearRelation();             // 清除 use-def 链
            inst->EraseFromManager();    // 从基本块中移除    
            delete inst;                  // 回收内存
        }
    }
    wait_del.clear(); // 清理集合（可选）
}