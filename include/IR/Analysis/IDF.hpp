#pragma once
#include "Dominant.hpp"
#include <set>

// iterated D F  迭代支配边界
class IDF{

public:
    void setLiveInBlocks(std::set<BasicBlock*>& Liveblock);
    void setDefiningBlocks(std::set<BasicBlock *>& Defblock);
    void calculate(std::vector<BasicBlock*>& IDFBlocks);
};