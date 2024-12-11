#include "CoreBaseClass.hpp"

//  BB类是用来构造CFG的主要类，需要去构造一些关系指令的构造  后期前端怎么设计怎么加
class BasicBlock:public Value
{
public:
    virtual ~BasicBlock() = default;
    BasicBlock();
    int num = 0;
    bool visited = false;
    bool reachable = false;
};