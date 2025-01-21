#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>

class Mem2RegPass
{
public:
    std::vector<BasicBlock*> FindDF();
    std::vector<BasicBlock*> insertPHINode();
    std::vector<BasicBlock*> vec;
};