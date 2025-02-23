#include "MemoryToRegister.hpp"
#include "../Analysis/Dominant.hpp"
#include <vector>
#include "./Passbase.hpp"
#include "../../lib/CoreClass.hpp"
#include "PassManager.hpp"

static bool promoteMemoryToRegister(Function* func)
{
    std::vector<AllocaInst*> Allocas;

    // 需要用到一个函数
    isAllocaPromotable();
}

class Mem2reg : public _PassManagerBase<Mem2reg,Function>{
public:
    Mem2reg(Function* function,_AnalysisManager &AM) :_func(function),_AM(AM)
    {}

    bool RunOnFunction()
    {
        return promoteMemoryToRegister(_func);
    }
private:
    Function *_func;
    _AnalysisManager &_AM;
};


