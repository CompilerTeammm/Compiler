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
    std::vector<BasicBlock*> blocks=func->GetBBs();
    for(auto* bb:func->){

    }
}