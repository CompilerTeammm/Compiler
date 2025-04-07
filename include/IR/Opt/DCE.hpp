
#pragma once
#include "Passbase.hpp"
#include "AnalysisManager.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../lib/MyList.hpp"

// DCE 中对于Instruction的一些判断是在class Instruction类中实现的
class DCE :public _PassBase<DCE,Function>
{
public:
    void run();
    DCE(Function* func,AnalysisManager* AM) 
        : _AM(AM), _func(func) {}
    
    bool eliminateDeadCode(Function* func);
                
    static bool isInstructionTriviallyDead(Instruction* I);

    bool DCEInstruction(Instruction *I,
                          std::vector<Instruction *> &WorkList);

    bool hasSideEffect(Instruction* inst);
private:
    Function* _func;
    AnalysisManager* _AM;
};