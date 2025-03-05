#pragma once
#include "../../include/Backend/RISCVMOperand.hpp"
#include "../../include/Backend/RISCVRegister.hpp"
//#include "../../include/lib/MagicEnum.hpp"
//to do：待我研究一下这个库是个啥
//有名字的操作数
class NamedMOperand:public RISCVMOperand{
    std::string name;
    public:
    std::string& GetName();
    NamedMOperand(std::string,RISCVType);
    void print()override;
}