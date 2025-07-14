// 实现 中端 到 后端的 转换
// llvm IR -> MIR 的转换
// Oprand
#pragma once
#include <list>
#include <memory>
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "../lib/MyList.hpp"
#include <string.h>
#include "../../Log/log.hpp"

/// 目标机器语言只有一种，RISCV，所以将 MIR -> RISCV
/// llvm 中端万物皆是 value ， 后端  万物都是 RISCVOp

// RISCVOp 作为一个基类操作数
// Imm 立即数  Register 寄存器   Label 标签 

// 常数，变量，运算   条件分支和循环  Undef
//  函数调用  指针， 数组

// 全局变量声明，自定义函数，int 类型，int 类型数组
// float 类型， float 类型数组  
// 赋值语句  算术运算，关系运算，逻辑运算 
// if else  while   类型转化
// 汇编：   rv64gc指令集

//  riscv64-unknown-elf-gcc -march=rv64gc -S try.c
// 	.file	"try.c"
// 	.option nopic
// 	.attribute arch, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0"
// 	.attribute unaligned_access, 0
// 	.attribute stack_align, 16
// 	.text
// 	.align	1
// 	.globl	main
// 	.type	main, @function
// main:
// 	addi	sp,sp,-16
// 	sd	s0,8(sp)
// 	addi	s0,sp,16
// 	li	a5,0
// 	mv	a0,a5
// 	ld	s0,8(sp)
// 	addi	sp,sp,16
// 	jr	ra
// 	.size	main, .-main
// 	.ident	"GCC: (13.2.0-11ubuntu1+12) 13.2.0"

// 	.file	"try.c"
// 	.option nopic
// 	.attribute arch, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0"
// 	.attribute unaligned_access, 0
// 	.attribute stack_align, 16
// 	.text
// 	.section	.text.startup,"ax",@progbits
// 	.align	1
// 	.globl	main
// 	.type	main, @function
// main:
// 	li	a0,0
// 	ret
// 	.size	main, .-main
// 	.ident	"GCC: (13.2.0-11ubuntu1+12) 13.2.0"

// 对于一条指令，我规定从左到右 分别是 0，1，2...

class RISCVFunction;
class RISCVBlock;

class RISCVOp 
{
public:
    enum Type
    {
        Global,
        Local
    };
private:
    std::string name;
    Type type;
public:
    RISCVOp() = default;
    RISCVOp(std::string _name,Type _type= Local):name(_name),type(_type) {}
    RISCVOp(float tmpf,Type _type= Local) : type(_type)  
    {
        uint32_t n;
        memcpy(&n, &tmpf, sizeof(float)); // 直接复制内存位模式
        name = std::to_string(n);
    }
    virtual ~RISCVOp() = default;
    
    template<typename T>
    T* as()
    {
        return static_cast<T*> (this);
    }

    void setName(std::string _string)
    {
        name = _string;
    }

    std::string& getName()
    {
        return name;
    }
};

// 该不该要呢？
class RISCVLabel:public RISCVOp 
{

};

class Imm: public RISCVOp
{
    Value* val;
    ConstIRFloat* fdata;
public:
    Imm(Value* _val): val(_val),RISCVOp(_val->GetName()) { }
    Imm(ConstIRFloat *_fdata) : fdata(_fdata),RISCVOp(_fdata->GetVal())
                                { }

    Imm(std::string name):RISCVOp(name) { }
};


// 虚拟实际寄存器封装到一起
// 我将 虚拟寄存器和 物理实际寄存器进行了封装
class Register:public RISCVOp
{
public:
    static int VirtualReg;
    enum FLAG 
    {
        real,
        vir
    };
    Register(std::string _name,bool Flag = vir):RISCVOp(_name) 
    { 
        if(Flag)
            VirtualReg++; 
    }
};

class RISCVInst:public RISCVOp,public Node<RISCVBlock,RISCVInst>
{
public:
    // 选择用Vector，因为方便遍历
    using op = std::shared_ptr<RISCVOp>;
private:
    std::vector<op> opsVec;

    using Instsptr = RISCVInst*;
    std::vector<Instsptr> Insts; 
public:
    // 并不是所有的指令都会产生对应关系，我这里根据手册全部写下
    enum ISA
    {
        // 算术运算的指令
        _add,
        _addi,
        _addiw, // w 结果截取为32位
        _addw,
        _sub,
        _subw,
        _mul,
        _div,
        _divu,
        _divuw,
        _divw,

        _mulw,
        _remw,
        _fsub_s,
        _fmv_s,
        _fcvt_w_s,
        _fcvt_s_w,
        _flt_s,
        _fle_s,
        _seqz,

        // 浮点数的指令,单精度浮点数 只支持float不支持其他类型
        _fabs_s,
        _fadd_s,
        _fdiv_s,
        _feq_s,
        _flw,
        _fmadd_s,
        _fmax_s,
        _fmin_s,
        _fmsub_s,
        _fmul_s,

        // 逻辑运算
        _and,
        _andi,
        _not,
        _or,
        _ori,
        _xor,
        _xori,

        // 移位操作
        // 左
        _sll,
        _slli,
        _slliw,
        _sllw,
        // 右
        _srl,
        _srli,
        _srliw,
        _srlw,

        // 比较，跳转指令
        _auipc,
        _beq,
        _beqz,
        _bge,
        _bgeu,
        _bgez,
        _bgt,
        _bgtu,
        _bgtz,
        _ble,
        _blez,
        _blt,
        _bltu,
        _bltz,
        _bne,
        _bnez,
        _bqe,

        // 调用
        _call,

        // 跳转
        _j,
        _jal,
        _jalr,
        _jr,
        _ret,

        // 取
        _la,
        _lb,
        _lbu,
        _ld,
        _li,
        _lui,
        _lw,
        _lwu,

        // 存
        _sw,
        _sd,

        // 一些伪指令
        _mv,
        _neg,
        _nop,

        // 环境断点
        _ebreak,
        _ecall,

        // 状态寄存器修改的指令
        _csrc,
        _csrci,
        _csrr,
        _csrrc,
        _csrrci,
        _csrrs,
        _csrrsi,
        _csrrw,
        _csrrwi,
        _csrs,
        _csrsi,
        _csrw,
        _csrwi,

        // 分类指令  感觉不太会有用到
        _fclass_d,
        _fclass_s,

        // 原子操作
        _amoadd_d,
        _amoadd_w,
        _amoand_d,
        _amoand_w,
        _amomax_d,
        _amomax_w,
        _amomaxu_d,
        _amomaxu_w,
        _amomin_d,
        _amomin_w,
        _amominu_d,
        _amominu_w,
        _amoor_d,
        _amoor_w,
        _amoswap_d,
        _amoswap_w,
        _amoxor_d,
        _amoxor_w,

        // 尾调用指令
        _tail,

        /// 压缩指令？？？
        /// c.

        _fmv_w_x,
        _fsw,
    };

    ISA opCode;
    
    enum Status 
    {
        ONE,
        MORE
    };

    Status status = ONE;
public:
    RISCVInst(ISA op) :opCode(op) { }

    ISA getOpcode() { return opCode;}

    void setThreeRigs(op op1,op op2) //addw
    {
        SetRegisterOp ("%." + std::to_string(Register::VirtualReg));
        opsVec.push_back(op1);
        opsVec.push_back(op2);
    }

    void setRetOp(Value* val)  // ret
    {
        SetRegisterOp("a0",Register::real);
        SetImmOp(val);
    }

    void setFRetOp(Value* val)  // ret
    {
        SetRegisterOp("fa0",Register::real);
        SetImmOp(val);
    }

    void setRetOp(op val)
    {
        SetRegisterOp("a0",Register::real);
        opsVec.push_back(val);
    }

    void setFRetOp(op val)
    {
        SetRegisterOp("fa0",Register::real);
        opsVec.push_back(val);
    }

    void setLoadOp()  // ld
    {
        SetRegisterOp ("%." + std::to_string(Register::VirtualReg));
    }

    void setVirLIOp(Value* val) // li
    {
        SetRegisterOp("%." + std::to_string(Register::VirtualReg));
        SetImmOp(val);
    }

    void setRealLIOp(Value* val) // li
    {
        SetRegisterOp("t0",Register::real);
        SetImmOp(val);
    }

    void setMVOp(RISCVInst* Inst)
    {
        SetRegisterOp("%." + std::to_string(Register::VirtualReg));
        auto reg = Inst->getOpreand(0);
        opsVec.push_back(reg);
    }

    // 涉及store 语句的需要单独处理
    void setStoreOp(RISCVInst* Inst)  // sw  sd
    {
        auto reg = Inst->getOpreand(0);
        if (reg == nullptr)
            LOG(ERROR,"the reg must not to be nullptr");
        
        opsVec.push_back(reg);
    }

    void setStoreStackOp(size_t offset)
    {
       opsVec.push_back(std::make_shared<RISCVOp> 
                       ("-" + std::to_string(offset) + "(s0)"));
    }

    void push_back(op Op) { opsVec.push_back(Op);}

    std::vector<op>& getOpsVec()  {  return opsVec;  }

    std::vector<Instsptr>& getInsts()  {  return Insts;  }

    op getOpreand(int i) {
        if (i > opsVec.size())
            assert("ops number is error");
        return opsVec[i];
    }

    void DealMore(Instsptr inst)
    {
        Insts.push_back(inst);
        status = MORE;
    }

    std::string ISAtoAsm();

    ~RISCVInst() = default;

    // 将操作每一个Op进行封装
    void SetRegisterOp(std::string&& str,bool Flag = Register::vir)
    {
        auto Regop = std::make_shared<Register> (str,Flag);
        opsVec.push_back(Regop);
    }

    void SetImmOp(std::string&& str)
    {
        auto Immop = std::make_shared<Imm> (str);
        opsVec.push_back(Immop);
    }

    void SetImmOp(Value* val)
    {
        std::shared_ptr<Imm> Immop = nullptr;
        if (val->GetType() == FloatType::NewFloatTypeGet()) {
            auto it = (val->as<ConstIRFloat>());
            if (it)
                Immop = std::make_shared<Imm> (val->as<ConstIRFloat>());
        }
        else
            Immop = std::make_shared<Imm> (val);
        opsVec.push_back(Immop);
        // std::cout << opsVec[1]->getName() << std:: endl;
    }
};


class RISCVBlock:public RISCVOp,public List<RISCVBlock, RISCVInst>, public Node<RISCVFunction, RISCVBlock>
{
    // std::string BBName;
    BasicBlock* cur_bb;
public:
    RISCVBlock(BasicBlock* bb,std::string name)
              :cur_bb(bb) , RISCVOp(name)   {    }

    ~RISCVBlock() = default;
};

// 栈帧的大小
class FrameObject:public RISCVOp
{
    std::vector<RISCVInst*> StoreInsts;
public:
    std::vector<RISCVInst*>& getStoreInsts()
    {
        return StoreInsts;
    }    

    void RecordStackMalloc(RISCVInst* inst)
    {
        StoreInsts.push_back(inst);
    }
};

// 与函数的栈帧相关
class RISCVPrologue:public RISCVOp
{
    using Instptr = std::shared_ptr<RISCVInst>;
    // std::vector<Instptr> proloInsts;
    typedef std::vector<Instptr> ProloInsts;
    ProloInsts proloInsts;
public:
    ProloInsts& getInstsVec()
    {
        return proloInsts;
    }
};

class RISCVEpilogue:public RISCVOp
{
    using Instptr = std::shared_ptr<RISCVInst>;
    // std::vector<Instptr> proloInsts;
    typedef std::vector<Instptr> EpilogueInsts;
    EpilogueInsts epiloInsts;
public:
    EpilogueInsts& getInstsVec()
    {
        return epiloInsts;
    }
};

// Function include BBs Name 
class RISCVFunction:public RISCVOp, public List<RISCVFunction, RISCVBlock>
{
    Function* func;
    RISCVBlock* exit;   
    std::shared_ptr<RISCVPrologue> prologue;
    std::shared_ptr<RISCVEpilogue> epilogue;

    //由所有函数记录该函数内的所有 storeInsts 语句 
    using lastStoreInstPtr = RISCVInst*;
    std::map<AllocaInst*,lastStoreInstPtr> StackStoreRecord;
    using matchLoadInstPtr = RISCVInst*;
    std::map<matchLoadInstPtr,AllocaInst*> StackLoadRecord;

    
    std::map<RISCVInst*,AllocaInst*> StoreInsts;
    std::vector<RISCVInst*> LoadInsts;
    std::vector<AllocaInst*> AllocaInsts;

public:
    RISCVFunction(Function* _func,std::string name)
                :func(_func),RISCVOp(name)     {   }

    std::vector<AllocaInst*>& getAllocas()  { return AllocaInsts;  }
    std::vector<RISCVInst*>& getLoadInsts()  {   return LoadInsts;    }
    std::map<RISCVInst*,AllocaInst*>& getStoreInsts() {   return StoreInsts;    }    
    std::map<AllocaInst*,lastStoreInstPtr>& getStoreRecord() {   return StackStoreRecord;   }
    std::map<matchLoadInstPtr,AllocaInst*>& getLoadRecord() {   return StackLoadRecord;   }

    void RecordStackMalloc(RISCVInst* inst,AllocaInst* alloca)
    {
        StoreInsts.emplace( std::make_pair(inst,alloca) );
    }

    void setPrologue(std::shared_ptr<RISCVPrologue>& it ) { prologue = it;}
    void setEpilogue(std::shared_ptr<RISCVEpilogue>& it ) { epilogue = it;}

    std::shared_ptr<RISCVPrologue>& getPrologue() { return prologue;}
    std::shared_ptr<RISCVEpilogue>& getEpilogue() { return epilogue;}
            
    ~RISCVFunction() = default;
};


class NamedRISCVOp:public RISCVOp
{

};

class OuterTag:public NamedRISCVOp
{

};

class RISCVObject:public NamedRISCVOp
{

};

class RISCVGlobalObject:public RISCVObject 
{

};

class RISCVTempFloatObject:public RISCVObject
{

};


