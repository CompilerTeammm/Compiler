#pragma once
#include "MIR.hpp"

class RegisterVec 
{
    std::vector<RealRegister*> intCallerRegVec;
    std::vector<RealRegister*> intCalleeRegVec;
    std::vector<RealRegister*> floatCallerRegVec;
    std::vector<RealRegister*> floatCalleeRegVec;

    RegisterVec();
    RegisterVec(const RegisterVec&) = delete;
    RegisterVec& operator=(const RegisterVec&) = delete;

public:
    static RegisterVec& GetRegVecs();
    
    std::vector<RealRegister*>& getIntCaller() { return intCallerRegVec;}
    std::vector<RealRegister*>& getIntCallee() { return intCalleeRegVec; }
    std::vector<RealRegister*>& getFloatCaller() { return floatCallerRegVec;}
    std::vector<RealRegister*>& getFloatCallee() { return floatCalleeRegVec; }
};