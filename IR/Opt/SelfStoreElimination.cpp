#include "../../include/IR/Opt/SelfStoreElimination.hpp"

bool SelfStoreElimination::run() {
    DFSOrder.clear();
    wait_del.clear();
    func->init_visited_block();
    //与DSE一样,构造DFS逆后序
    OrderBlock(func->front);
    std::reverse(DFSOrder.begin(), DFSOrder.end());

    std::unordered_map<Value*, std::vector<User*>> storeMap;
    CollectStoreInfo(storeMap); // 收集“每个地址”上的所有 store
    CheckSelfStore(storeMap); // 保守判断冗余并插入待删列表

    for (auto &[key, vec] : storeMap) {
        for (auto inst : vec)
            wait_del.insert(inst);
    }

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
                    if (auto* alloca = dynamic_cast<AllocaInst*>(gep->GetUserUseList()[0]->usee)) {
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
}

void SelfStoreElimination::CheckSelfStore(std::unordered_map<Value*, std::vector<User*>>& storeMap){
    for (auto* bb : DFSOrder){
        for (auto* inst : *bb){
            // case1: store(load(x), x)
            if (auto* store = dynamic_cast<StoreInst*>(inst)){
                if (auto* load = dynamic_cast<LoadInst*>(store->GetOperand(0))){
                    Value* load_src =load->GetUserUseList()[0]->usee;
                    Value* store_dst = store->GetOperand(1);

                    if (load_src == store_dst){
                        if (auto* alloca = dynamic_cast<AllocaInst*>(load_src)) {
                            storeMap.erase(alloca);
                        }else{
                            storeMap.erase(load_src);
                        }
                    }
                }else{
                    storeMap.erase(store->GetOperand(0));
                }
            }
            // case2: GEP 被其他未知指令使用
            else if(auto* gep=dynamic_cast<GepInst*>(inst)){
                if(auto* base=dynamic_cast<AllocaInst*>(gep->GetUserUseList()[0]->usee)){
                    for(auto* u:gep->GetValUseList()){
                        User* user = u->GetUser();
                        if(!dynamic_cast<StoreInst*>(user)&& !dynamic_cast<GepInst*>(user)){
                            storeMap.erase(base);
                            break;   
                        }
                    }
                }
            }
            // case3: memcpy to memory
            else if (auto* call = dynamic_cast<CallInst*>(inst)){
                std::string name = call->GetOperand(0)->GetName();
                if (name == "llvm.memcpy.p0.p0.i32"){
                    Value* dst = call->GetOperand(1);
                    if(auto* gep=dynamic_cast<GepInst*>(dst)){
                        if(auto* alloca=dynamic_cast<AllocaInst*>(gep->GetUserUseList()[0]->usee)){
                            storeMap[alloca].push_back(call);
                        }
                    }else if(auto* alloca = dynamic_cast<AllocaInst*>(dst)){
                        storeMap[alloca].push_back(call);
                    }
                }else{
                    // conservatively remove all
                    for (auto& [val, list] : storeMap) {
                        storeMap.erase(val);
                    }
                    return;
                }
            }
            // case4: 非 store 的其他指令对 alloca 使用
            else{
                for (auto& use : inst->GetUserUseList()) {
                    Value* v = use->usee;
                    storeMap.erase(v);
                }
            }
                
        }   
    }
}

void SelfStoreElimination::removeInsts() {
    for (auto* user : wait_del) {
        if (auto* inst = dynamic_cast<Instruction*>(user)) {
            inst->ClearRelation();             // 清除 use-def 链
            inst->EraseFromManager();    // 从基本块中移除    
            delete inst;                  // 回收内存
        }
    }
    wait_del.clear(); // 清理集合（可选）
}