#include "../include/backend/RISCVMIR.hpp"
#include "../include/backend/RISCVMOperand.hpp"
#include "../include/backend/RISCVRegister.hpp"
#include "../include/backend/Liveness.hpp"
#include "../util/my_stl.hpp"
#include <algorithm>
#include <map>
#include <regex>
#include <set>
using BlockInfo = Liveness;
using InterVal = LiveInterval;
using OpType = RISCVMIR::RISCVISA;
void BlockInfo::RunOnFunction(){
    for(RISCVBasicBlock *block:*m_func){
        BlockLivein[block].clear();
        BlockLiveout[block].clear();
        GetBlockLivein(block);
        GetBlockLiveout(block);
    }
    bool modified=true;//是否需要迭代
    while(modified){
        modified=false;
        //逆序遍历计算livein liveout
        for(auto block=m_func->rbegin();block!=m_func->rend();--block){
            RISCVBasicBlock *cur = *block;
            std::unordered_set<MOperand> oldin = BlockLivein[cur];//记录当前livein
            GetBlockLiveout(cur);//计算当前基本块liveout
            BlockLivein[cur] = BlockLiveout[cur];//先将out赋值给in
            GetBlockLivein(cur);//计算livein
            if (BlockLivein[cur] != oldin)
                modified = true;
        }
    }
}