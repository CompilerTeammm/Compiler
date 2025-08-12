#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include "AnalysisManager.hpp"

class DAE:public _PassBase<DAE, Module>{
private:
    std::vector<std::pair<Value *, int>> wait_del;
    Module *mod;
    AnalysisManager &AM;
    bool DetectDeadargs(Function *func);
    void NormalHandle(Function* func);
    void Handle_SameArgs(Function* func);
    public:
    bool run() override;    
    DAE(Module *m,AnalysisManager &_AM) : mod(m),AM(_AM){}
    ~DAE()=default;
};