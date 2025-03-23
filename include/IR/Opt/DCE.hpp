
#include "Passbase.hpp"
#include "AnalysisManager.hpp"
#include "CoreClass.hpp"
#include "CFG.hpp"
#include "MyList.hpp"

class DCE :public _PassBase<DCE,Function>
{
public:
    void run();
    DCE(Function* func,AnalysisManager* AM) 
        : _AM(AM), _func(func) {}
    
    bool eliminateDeadCode(Function* func);

    bool IsDCEInstruction();
private:
    Function* _func;
    AnalysisManager* _AM;
};