#include "../include/MyBackend/RegisterVec.hpp"

using realReg=Register::realReg;
RegisterVec::RegisterVec()
{
    realReg regOp = realReg::a0;
    auto appendReg = [&](std::vector<Register*>& vec) {
        vec.push_back(Register::GetRealReg(regOp));
        regOp = realReg(regOp + 1);
    };
    // int      t2   a0-a7 t3-t6  s1-s11       24
    // t0 & t1 will not be regalloced, and it is real temp register
    while(regOp<=realReg::a7) {
      appendReg(intRegVec);
    }

    intRegVec.push_back(Register::GetRealReg(realReg::s1));
    regOp = realReg::s2;
    while(regOp<=realReg::s11) {
      appendReg(intRegVec);
    }
    
    intRegVec.push_back(Register::GetRealReg(realReg::t2));
    regOp = realReg::t3;
    while(regOp<=realReg::t6) {
      appendReg(intRegVec);
    }
    // float  ft0-ft11  fa0-fa7  fs0-fs11      32
    regOp = realReg::ft0;
    while(regOp<=realReg::ft11) {
      appendReg(floatRegVec);
    }
    regOp = realReg::fa0;
    while(regOp<=realReg::fa7) {
      appendReg(floatRegVec);
    }
    regOp = realReg::fs0;
    while(regOp<=realReg::fs11) {
      appendReg(floatRegVec);
    }

    // caller
    callerRegVec.push_back(Register::GetRealReg(realReg::t2));
    regOp = realReg::a0;
    while(regOp<=realReg::a7) {
      appendReg(callerRegVec);
    }
    regOp = realReg::t3;
    while(regOp<=realReg::t6) {
      appendReg(callerRegVec);
    }
    regOp = realReg::ft0;
    while(regOp <= realReg::ft11) {
      appendReg(callerRegVec);
    }

    // // reglist_test
    // regOp = realReg::a0;
    // while(regOp<=realReg::a3) {
    //   appendReg(reglist_test);
    // }
}

RegisterVec& RegisterVec::GetRegVecs()
{
    static RegisterVec regVec;
    return regVec;
}

