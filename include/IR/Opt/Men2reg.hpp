#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>
#include "./Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "PassManager.hpp"

// Mem2reg的实现需要大量的数据结构取供给
class Mem2reg;
class Mem2reg : public _PassBase<Mem2reg ,Function>
{
public:
    Mem2reg(Function* function, _AnalysisBase &AM) :_func(function),_AM(AM)
    {}

    bool RunOnFunction()
    {
        return promoteMemoryToRegister(_func);
    }
private:
    Function *_func;
    _AnalysisBase<,Function> &_AM;
};


