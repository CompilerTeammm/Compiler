#pragma once
#include "Passbase.hpp"
#include "AnalysisManager.hpp"
#include "PassManager.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../lib/MyList.hpp"
#include "../Analysis/SideEffect.hpp"

class DSE:public _PassBase<DSE,Function>{
    private:
    Function* func;
    DominantTree* tree;
    SideEffect* se;
    public:
    bool run();
    DSE(Function* _func,DominantTree* _tree,SideEffect* _se): tree(_tree),func(_func),se(_se) {}//待补参数aa,se
    ~DSE()=default;
    
    //待删除指令集合
    std::set<Instruction*> DeadStores;
    //基本块处理顺序(逆支配顺序)
    std::vector<BasicBlock*> DFSOrder;

    //初始化块遍历顺序
    void initDFSOrder();

    //处理单个基本块
    bool processBlock(BasicBlock* bb);

    //判断存储是否为死存储
    bool isDeadStore(StoreInst* st,BasicBlock* bb);
    bool isDeadAfter(Value* addr, BasicBlock* bb,
        std::unordered_set<BasicBlock*>& visited,
        Instruction* start);
    bool hasSideEffect(Instruction* inst);
};