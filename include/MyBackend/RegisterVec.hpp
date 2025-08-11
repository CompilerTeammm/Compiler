#pragma once
#include "MIR.hpp"

class RegisterVec 
{
    std::vector<RealRegister*> intRegVec;
    std::vector<RealRegister*> floatRegVec;
    std::vector<RealRegister*> callerRegVec;

    RegisterVec();
    RegisterVec(const RegisterVec&) = delete;
    RegisterVec& operator=(const RegisterVec&) = delete;

public:
    static RegisterVec& GetRegVecs();
    std::vector<RealRegister*>& GetintRegVec() { return intRegVec; }
    std::vector<RealRegister*>& GetfloatRegVec() { return floatRegVec; }
};