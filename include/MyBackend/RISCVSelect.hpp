#pragma once
#include "RISCVContext.hpp"

/// Instruction Selection 最简单的宏匹配
/// 我最开始想法是直接写select Insts 的函数到 TransFunction 
/// 后来觉得可能分离出来之后做优化会容易一些
/// InstSelect 不如叫 InstsTrans

class RISCVSelect 
{
    std::shared_ptr<RISCVContext>& ctx;
public:
    RISCVSelect(std::shared_ptr<RISCVContext>& _ctx)
                : ctx(_ctx)   {  } 


    // 匹配所以的指令
    void MatchAllInsts(BasicBlock* bb);
};