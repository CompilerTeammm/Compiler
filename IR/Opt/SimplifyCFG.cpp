#include "../../include/IR/Opt/SimplifyCFG.hpp"

bool SimplifyCFG::run() {
    return SimplifyCFGFunction(func);
}

bool SimplifyCFG::SimplifyCFGFunction(Function* func){
    bool changed=false;

    //function子优化
    changed |= removeUnreachableBlocks(func);
    changed |= mergeEmptyReturnBlocks(func);
    //basicblock子优化
    std::vector<BasicBlock*> blocks;
    for(auto& bb_ptr:func->GetBBs()){
        blocks.push_back(bb_ptr.get());//从shared_ptr提取裸指针
    }
    for(auto* bb:blocks){
        changed|=SimplifyCFGBasicBlock(bb);
    }
    return changed;
}

bool SimplifyCFG::SimplifyCFGBasicBlock(BasicBlock* bb){
    bool changed=false;
    changed |=mergeBlocks(bb);
    changed |=simplifyBranch(bb);
    changed |=eliminateTrivialPhi(bb);

    return changed;
}

//删除不可达基本块(记得要把phi引用到的也进行处理)
bool removeUnreachableBlocks(Function* func){
    std::unordered_set<BasicBlock*> reachable;//存储可达块
    std::stack<BasicBlock*> bbstack;

    auto entry=func->GetFront();
    bbstack.push(entry);
    reachable.insert(entry);

    //DFS
    while(!bbstack.empty()){
        BasicBlock* bb=bbstack.top();
        bbstack.pop();
        for(auto& succ:bb->GetNextBlocks()){
            if(reachable.insert(succ).second){
                bbstack.push(succ);
            }
        }
    }

    bool changed=false;
    //遍历所有bb,移除不可达者
    auto& BBList=func->GetBBs();
    for(auto it=BBList.begin();it!=BBList.end();){
        BasicBlock* bb=it->get();
        if(reachable.count(bb)==0){
            for(auto pred:bb->GetPredBlocks()){
                pred->RemoveNextBlock(bb);
            }
            for(auto succ:bb->GetNextBlocks()){
                succ->RemovePredBlock(bb);
            }
            it=BBList.erase(it);
            changed=true;
        }else{
            ++it;
        }
    }
    return changed;
}

//合并空返回块
bool SimplifyCFG::mergeEmptyReturnBlocks(Function* func){
    auto& BBs=func->GetBBs();
    std::vector<BasicBlock*> returnBlocks;

    //收集所有空ret块
    for(auto& bbPtr:BBs){
        BasicBlock* bb=bbPtr.get();
        if(bb->Size()==1&&bb->front->id==Instruction::Op::Ret){
            returnBlocks.push_back(bb);
        }
    }
    //合并空ret块
    if(returnBlocks.size()<=1){
        return false;
    }
    //选定第一个作为公共返回块
    BasicBlock* commonRet=returnBlocks.front();

    //重定向其他返回块的前驱到commonRet,并清理phi
    for(size_t i=1;i<returnBlocks.size();++i){
        BasicBlock* redundant=returnBlocks[i];
        for(auto* pred:)
    }
}