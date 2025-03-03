#pragma once
#include "../../include/Backend/RISCVType.hpp"
#include "../../include/lib/BaseCFG.hpp"

class Register;

class RISCVMOperand {
    RISCVType tp;
    public:
    RISCVMOperand(RISCVType _tp) : tp(_tp) {};
    RISCVType GetType(){
        return tp;}
    virtual void print()=0;
    
    template<typename T>
    T* as(){
        return dynamic_cast<T*>(this);
    }
    Register* ignoreLA();
};

//立即数封装类
class Imm : public RISCVMOperand {
    ConstantData* data;
    public:
    Imm(ConstantData* );
    ConstantData* GetData();
    static Imm* GetImm(ConstantData*);
    void print()final;
}