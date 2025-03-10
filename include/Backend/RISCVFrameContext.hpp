#pragma once
#include "../../include/Backend/RISCVMOperand.hpp"
#include "../../include/Backend/RISCVRegister.hpp"
//#include "../../include/lib/MagicEnum.hpp"
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
//全局
class RISCVGlobalObject:public RISCVObject{
    public:
    RISCVGlobalObject(Type*,std::string name);
    void print()override;
};
//临时浮点对象(other？)
class RISCVTempFloatObject:public RISCVObject{
    public:
    RISCVTempFloatObject(std::string name);
    void print()override;
};

//栈帧对象
class RISCVFrameObject:public RISCVMOperand{
    size_t begin_addr_offsets=0;//起始偏移量
    size_t end_addr_offsets=0;//结束偏移量

    StackRegister* reg;
    size_t size=0;
    std::string name;
    RISCVType contexttype;

    public:
    RISCVFrameObject();//构造函数
    RISCVFrameObject(Value*);
    RISCVFrameObject(VirRegister*);
    RISCVFrameObject(PhyRegister*);

    void GenerateStackRegister(int);//生成栈寄存器
    size_t GetFrameObjSize();//返回栈帧对象大小
    size_t GetBeginAddOffsets();
    size_t GetEndAddOffsets();
    RISCVType GetContextType();
    void SetBeginAddOffsets(size_t);
    void SetEndAddOffsets(size_t);
    StackRegister*& GetStackReg();//返回栈寄存器的引用
    void print()override;
};
//栈寄存器
class StackRegister:public Register{
    int offset;//栈寄存器偏移量
    RISCVFrameObject* parent=nullptr;//指向父栈帧对象
    Register* reg=nullptr;//指向寄存器

    public:
    StackRegister(RISCVFrameObject*,PhyRegister::PhyReg,int);//初始化
    StackRegister(RISCVFrameObject*,VirRegister*,int);
    StackRegister(PhyRegister::PhyReg,int);
    StackRegister(VirRegister*,int);

    std::string GetName(){
        return rname;//返回寄存器名
    }
    int GetOffset(){
        return offset;//返回偏移量
    }
    RISCVFrameObject*&  GetParent();
    VirRegister* GetVreg();
    Register*& GetReg();

    void SetPreg(PhyRegister*&);
    void SetReg(Register*);
    void SetOffset(int);
    void print()final;
    bool isPhysical()final;
};