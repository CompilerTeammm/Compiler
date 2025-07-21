#pragma once
#include "MIR.hpp"

class RegisterVec 
{
    std::vector<Register*> intRegVec;
    std::vector<Register*> floatRegVec;
    std::vector<Register*> callerRegVec;

    RegisterVec();
    RegisterVec(const RegisterVec&) = delete;
    RegisterVec& operator=(const RegisterVec&) = delete;

public:
    static RegisterVec& GetRegVecs();
    std::vector<Register*>& GetintRegVec() { return intRegVec; }
    std::vector<Register*>& GetfloatRegVec() { return floatRegVec; }
};