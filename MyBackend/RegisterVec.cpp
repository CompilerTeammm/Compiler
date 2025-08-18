#include "../include/MyBackend/RegisterVec.hpp"

using realReg=RealRegister::realReg;
RegisterVec::RegisterVec()
{
    realReg regOp = realReg::a0;
    auto appendReg = [&](std::vector<RealRegister*>& vec) {
        vec.push_back(RealRegister::GetRealReg(regOp));
        regOp = realReg(regOp + 1);
    };
    // int      t2   a0-a7 t3-t6  s1-s11       24
    // t0 & t1 will not be regalloced, and it is real temp register
    while ( regOp != realReg::s1)
    {
        appendReg(intCallerRegVec);
    }
    regOp = realReg::s1;
    while ( regOp != realReg::fa0)
    {
        appendReg(intCalleeRegVec);
    }
    regOp = realReg::fa0;
    while ( regOp != realReg::fs0)
    {
        appendReg(floatCallerRegVec);
    }
    regOp = realReg::fs0;
    while ( regOp != realReg::_NULL)
    {
        appendReg(floatCallerRegVec);
    }
}

RegisterVec& RegisterVec::GetRegVecs()
{
    static RegisterVec regVec;
    return regVec;
}

