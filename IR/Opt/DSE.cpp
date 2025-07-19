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
}

bool DSE::processBlock(BasicBlock* bb){
    bool changed=false;
    std::set<Value*> liveAddr;//活跃的地址set

    //倒序遍历该块的指令
    for(auto it=bb->rbegin();it!=bb->rend();++it){
        Instruction* inst = *it;

        if(inst->id==Instruction::Op::Store){
            Value* addr = inst->GetOperand(1);
            if(!liveAddr.count(addr)){
                //未被使用过,是死store
                DeadStores.insert(inst);
                changed = true;
            }else{
                liveAddr.erase(addr);
            }
        }else if(inst->id==Instruction::Op::Load){
            Value* addr=inst->GetOperand(0);
            liveAddr.insert(addr);
        }else if(hasSideEffect(inst)){
            //有副作用的保守处理,清空
            liveAddr.clear();
        }
    }
    return changed;
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