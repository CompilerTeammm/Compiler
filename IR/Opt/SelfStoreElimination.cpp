#include "../../include/IR/Opt/SelfStoreElimination.hpp"

bool SelfStoreElimination::run() {
    dfsOrder_.clear();
    pendingRemoval_.clear();
    func_->init_visited_block();
    
    orderBlocks(func_->front);
    std::reverse(dfsOrder_.begin(), dfsOrder_.end());

    std::unordered_map<Value*, std::vector<User*>> storeMap;
    collectStores(storeMap);
    identifySelfStores(storeMap);

    removeInstructions();

    return !pendingRemoval_.empty();
}

void SelfStoreElimination::orderBlocks(BasicBlock* bb) {
    if (bb->visited) return;
    bb->visited = true;

    auto* node = domTree_->getNode(bb);
    for (auto succNode : node->succNodes) {
        orderBlocks(succNode->curBlock);
    }

    dfsOrder_.push_back(bb);
}

void SelfStoreElimination::collectStores(std::unordered_map<Value*, std::vector<User*>>& storeMap) {
    for (auto* bb : dfsOrder_) {
        for (auto* inst : *bb) {
            if (auto store = dynamic_cast<StoreInst*>(inst)) {
                Value* dst = store->GetOperand(1);

                if (auto* gep = dynamic_cast<GepInst*>(dst)) {
                    Value* base = gep->GetOperand(0);
                    if (auto* alloca = dynamic_cast<AllocaInst*>(base)) {
                        storeMap[alloca].push_back(store);
                    }
                } else if (auto* alloca = dynamic_cast<AllocaInst*>(dst)) {
                    storeMap[alloca].push_back(store);
                } else if (!dst->isGlobal()) {
                    storeMap[dst].push_back(store);
                }
            }
        }
    }
}

static bool isSameGEP(Value* a, Value* b) {
    auto* gepA = dynamic_cast<GepInst*>(a);
    auto* gepB = dynamic_cast<GepInst*>(b);
    if (!gepA || !gepB) return false;

    if (gepA->GetOperandNums() != gepB->GetOperandNums()) return false;

    for (int i = 0; i < gepA->GetOperandNums(); ++i) {
        if (gepA->GetOperand(i) != gepB->GetOperand(i)) return false;
    }
    return true;
}

void SelfStoreElimination::identifySelfStores(std::unordered_map<Value*, std::vector<User*>>& storeMap) {
    std::set<Value*> unsafeAddrs;

    for (auto* bb : dfsOrder_) {
        for (auto* inst : *bb) {
            // case 1: 自写入 store(load(x), x)
            if (auto* store = dynamic_cast<StoreInst*>(inst)) {
                Value* src = store->GetOperand(0);
                Value* dst = store->GetOperand(1);

                if (auto* load = dynamic_cast<LoadInst*>(src)) {
                    Value* loadSrc = load->GetOperand(0);
                    if (loadSrc == dst || isSameGEP(loadSrc, dst)) {
                        if (!pendingRemoval_.count(store)) {
                            pendingRemoval_.insert(store);  // 直接删除该指令，不清除 map
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
                            unsafeAddrs.insert(base); // 标记该地址不安全
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
                    unsafeAddrs.insert(v);
                }
            }
        }
    }

    // 删除所有 unsafe 地址
    for (Value* addr : unsafeAddrs) {
        storeMap.erase(addr);
    }

    // case 5: 对剩余 safe 地址，保留最后一次写，前面的都冗余
    for (auto& [addr, stores] : storeMap) {
        for (size_t i = 0; i + 1 < stores.size(); ++i) {
            if (!pendingRemoval_.count(stores[i])) {
                pendingRemoval_.insert(stores[i]);
            }
        }
    }
}



void SelfStoreElimination::removeInstructions() {
    for (auto* user : pendingRemoval_) {
        if (auto* inst = dynamic_cast<Instruction*>(user)) {
            if (!inst->GetParent()) {
                std::cerr << "[SSE] Warning: Attempt to remove already-erased instruction: "
                          << inst->GetName() << "\n";
                continue;
            }
            inst->ClearRelation();
            inst->EraseFromManager();
            delete inst;
        }
    }
    pendingRemoval_.clear();
}
