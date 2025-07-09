#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"

class SimplifyInst: public _PassBase<SimplifyInst,Function>{
    private:
    Function* func;
    DominantTree* tree;
    public:
    bool run() override;
    SimplifyInst(Function* _func,DominantTree* _tree):func(_func),tree(_tree){}
    ~SimplifyInst()=default;

    bool simplifyInst(Function* func);
};