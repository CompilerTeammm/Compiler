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
    void print()override;//override 是 C++11 引入的一个关键字，用于在派生类中显式地重写（覆盖）基类的虚函数。
};
//外部标签(函数标签等)
class OuterTag:public NamedMOperand{
    public:
    OuterTag(std::string);
    static OuterTag* GetOuterTag(std::string);
};


//riscv对象
class RISCVObject:public NamedMOperand{
    protected:
    Type* tp;
    bool local;//局部？
    public:
    RISCVObject(Type*,std::string);
    RISCVObject(std::string);
};
//这里学长继承了一下，在想是否可以单独写，这样protected中的local可以单独定值。
//全局
class RISCVGlobalObject:public RISCVObject{
    public:
    RISCVGlobalObject(Type*,std::string name);
    void print()override;
}