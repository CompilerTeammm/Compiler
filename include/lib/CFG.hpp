#include "CoreBaseClass.hpp"
#include <vector>
#include "SymbolTable.hpp"
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

// Function 类是用来储存bb类的
class Function:public Value
{
private:
    std::vector<BasicBlock*> bbs;
    int BB_num = 0;
public:
    int Get_BB_num()  {return BB_num;}
    std::vector<BasicBlock*>&  GetBBVector() {return &bbs;} 
};


class Function;
class BasicBlock;
class Moudle:public SymbolTable // 学长继承的是符号表
{
private:
    using FunctionPtr = std::unique_ptr<Function*>;
    std::vector<FunctionPtr> ptr_ls;
public:
    std::vector<FunctionPtr> GetFunptrList() {return ptr_ls;} 
    Moudle() = default;
};