#include "SymbolTable.hpp"
#include <memory>
#include <vector>
#include"Function.hpp"
#include"BasicBlock.hpp"

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