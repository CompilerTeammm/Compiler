#include "../../include/IR/Opt/DSE.hpp"

bool DSE::run(){
    DeadStores.clear();
    initDFSOrder();//初始化遍历顺序

    bool changed=false;

    for(auto* bb:DFSOrder){
        changed |= processBlock(bb);//对每个块做DSE
    }

    //执行删除
    for(auto* inst : DeadStores){
        inst->EraseFromManager();//统一删除死store
    }
    return changed;
}

void DSE::initDFSOrder(){
    DFSOrder.clear();
    if(!tree){
        return;
    }
    //使用树的逆后序DFS(保证使用者在前,定义在后)
    //tree->getReverseDFSOrder(DFSOrder); 
    //试试能不能直接写到这个里面
    std::set<BasicBlock*> visited;

    //递归后序遍历支配树

    std::function<void(BasicBlock*)> dfs =[&](BasicBlock* bb){
        if(!bb || visited.count(bb)) return;
        visited.insert(bb);
        auto idomChildren=tree->getIdomVec(bb);//支配树的孩子

        for(auto* child:idomChildren){
            dfs(child);
        }
        DFSOrder.push_back(bb);//后序插入
    };

    dfs(func->GetBBs()[0].get());//默认入口块

    std::reverse(DFSOrder.begin(), DFSOrder.end());
}

bool DSE::processBlock(BasicBlock* bb){
    bool changed=false;

    //倒序遍历该块的指令
    for(auto it=bb->rbegin();it!=bb->rend();++it){
        Instruction* inst = *it;

        if(inst->id==Instruction::Op::Store){
            auto* store=dynamic_cast<StoreInst*>(inst);
            if(isDeadStore(store,bb)){
                DeadStores.insert(inst);
                changed=true;
            }
        }
    }
    return changed;
}

bool DSE::isDeadStore(StoreInst* st,BasicBlock* bb){
    Value* addr=st->GetOperand(1);
    std::unordered_set<BasicBlock*> visited;
    auto it=st->GetNextNode();//store 下条指令,这个方法对嘛

    return isDeadAfter(addr, bb, visited, it); // 下一步我们要做的跨块判断

}
bool DSE::isDeadAfter(Value* addr, BasicBlock* bb,
    std::unordered_set<BasicBlock*>& visited,
    Instruction* start){

        if (visited.count(bb)) return true; // 避免死循环
        visited.insert(bb);

        //从当前指令开始扫描
        for (Instruction* inst = start; inst != nullptr; inst = inst->GetNextNode()){
            if (inst->id == Instruction::Op::Load){
                Value* loadAddr=inst->GetOperand(0);
                if(loadAddr==addr){
                    return false;
                }
            }else if(inst->id == Instruction::Op::Store){
                Value* storeAddr=inst->GetOperand(1);
                if(storeAddr==addr){
                    return true;
                }
            }else if(hasSideEffect(inst)){
                //保守处理,如果有副作用就不剩
                return false;
            }
        }

        //如果到此还没明确结果,那就看后继块继续搜索
        for(auto* succ:bb->GetNextBlocks()){
            Instruction* nextStart=succ->GetFront();
            if(!isDeadAfter(addr,succ,visited,nextStart)){
                return false;//有一条路径不能删,就不能删
            }
        }
        return true;//所有路径都能删
    }
bool DSE::hasSideEffect(Instruction* inst){
    switch(inst->id){
        case Instruction::Op::Store:
        case Instruction::Op::Call:
        case Instruction::Op::Ret:
        case Instruction::Op::Memcpy:
            return true;
        default:
            return false;    
    }
}