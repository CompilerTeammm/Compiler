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
#include <string>
#include "../../Log/log.hpp"
#include "RISCVType.hpp"

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
private:
    std::string name;
public:
    RISCVOp() = default;
    RISCVOp(std::string _name):name(_name)  { }   
    virtual ~RISCVOp() = default;
    
    template<typename T>
    T* as() {
        return static_cast<T*> (this);
    }
    void setName(std::string _string) {  name = _string;  }
    std::string& getName() {   return name;  }
};

class Imm: public RISCVOp
{
    RISCVType type;
    ConstantData* data;
    public:
    Imm(ConstantData* _data);
    ConstantData* getData();
    void ImmInit();
    static std::shared_ptr<Imm> GetImm(ConstantData* _data);
};

class stackOffset:public RISCVOp
{
    Value *val;
public:
    stackOffset(Value *_val) : val(_val), RISCVOp(_val->GetName()) {}
    stackOffset(std::string name) : RISCVOp(name) {}
};

// 虚拟实际寄存器封装到一起
// 我将 虚拟寄存器和 物理实际寄存器进行了封装
class Register:public RISCVOp
{
public:
    static int VirtualReg;
    enum FLAG {
        real,   // 0
        vir     // 1
    };
    enum realReg{
        // int  ABI name
        zero,ra,sp,gp,tp,t0,t1,t2,
        s0,s1,a0,a1,a2,a3,a4,a5,a6,
        a7,s2,s3,s4,s5,s6,s7,s8,s9,
        s10,s11,t3,t4,t5,t6,

        x0=zero,x1=ra,x2=sp,x3=gp,x4=tp,x5=t0,x6=t1,x7=t2,
        x8=s0,x9=s1,x10=a0,x11=a1,x12=a2,x13=a3,x14=a4,x15=a5,
        x16=a6,x17=a7,x18=s2,x19=s3,x20=s4,x21=s5,x22=s6,x23=s7,
        x24=s8,x25=s9,x26=s10,x27=s11,x28=t3,x29=t4,x30=t5,x31=t6,
        
        // float
        ft0,ft1,ft2,ft3,ft4,ft5,ft6,ft7,ft8,ft9,ft10,ft11,
        fs0,fs1,fs2,fs3,fs4,fs5,fs6,fs7,fs8,fs9,fs10,fs11,
        fa0,fa1,fa2,fa3,fa4,fa5,fa6,fa7, _NULL,
    } realRegop;
    Register(std::string _name,bool Flag = vir,int _Fflag=0 );
    Register(realReg _Regop,bool Flag = real,int _Fflag=0);
    int Fflag;
    int RVflag;
    bool IsrealRegister() { return RVflag == real; }  // vir or real Reg
    bool IsFflag() { return Fflag == 1; }           // FloatReg or  IntReg
    realReg getRegop();
    void setRVflag() { RVflag = real; }
    void reWirteRegWithReal(Register* );
    static Register* GetRealReg(realReg);
    std::string realRegToString(realReg reg) {
    switch(reg) {
        // 基础整数寄存器
        case realReg::zero: return "zero";
        case realReg::ra:   return "ra";
        case realReg::sp:   return "sp";
        case realReg::gp:   return "gp";
        case realReg::tp:   return "tp";
        case realReg::t0:   return "t0";
        case realReg::t1:   return "t1";
        case realReg::t2:   return "t2";
        case realReg::s0:   return "s0";
        case realReg::s1:   return "s1";
        case realReg::a0:   return "a0";
        case realReg::a1:   return "a1";
        case realReg::a2:   return "a2";
        case realReg::a3:   return "a3";
        case realReg::a4:   return "a4";
        case realReg::a5:   return "a5";
        case realReg::a6:   return "a6";
        case realReg::a7:   return "a7";
        case realReg::s2:   return "s2";
        case realReg::s3:   return "s3";
        case realReg::s4:   return "s4";
        case realReg::s5:   return "s5";
        case realReg::s6:   return "s6";
        case realReg::s7:   return "s7";
        case realReg::s8:   return "s8";
        case realReg::s9:   return "s9";
        case realReg::s10:  return "s10";
        case realReg::s11:  return "s11";
        case realReg::t3:   return "t3";
        case realReg::t4:   return "t4";
        case realReg::t5:   return "t5";
        case realReg::t6:   return "t6";

        // 浮点寄存器
        case realReg::ft0:  return "ft0";
        case realReg::ft1:  return "ft1";
        case realReg::ft2:  return "ft2";
        case realReg::ft3:  return "ft3";
        case realReg::ft4:  return "ft4";
        case realReg::ft5:  return "ft5";
        case realReg::ft6:  return "ft6";
        case realReg::ft7:  return "ft7";
        case realReg::ft8:  return "ft8";
        case realReg::ft9:  return "ft9";
        case realReg::ft10: return "ft10";
        case realReg::ft11: return "ft11";
        case realReg::fs0:  return "fs0";
        case realReg::fs1:  return "fs1";
        case realReg::fs2:  return "fs2";
        case realReg::fs3:  return "fs3";
        case realReg::fs4:  return "fs4";
        case realReg::fs5:  return "fs5";
        case realReg::fs6:  return "fs6";
        case realReg::fs7:  return "fs7";
        case realReg::fs8:  return "fs8";
        case realReg::fs9:  return "fs9";
        case realReg::fs10: return "fs10";
        case realReg::fs11: return "fs11";
        case realReg::fa0:  return "fa0";
        case realReg::fa1:  return "fa1";
        case realReg::fa2:  return "fa2";
        case realReg::fa3:  return "fa3";
        case realReg::fa4:  return "fa4";
        case realReg::fa5:  return "fa5";
        case realReg::fa6:  return "fa6";
        case realReg::fa7:  return "fa7";

        default:
            throw std::invalid_argument("Invalid realReg value");
        }
    }
};

class VirRegister:public Register
{

};

class RealRegister:public Register
{

};

// 地址操作符
class RISCVAddrOp:public RISCVOp 
{
public:
    RISCVAddrOp(std::string name) :RISCVOp(name) { }
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
    void reWriteISA() {
        if (opCode == _bne)
            opCode = _beq;
        else if (opCode == _beq)
            opCode = _bne;
        else if (opCode == _bge)
            opCode = _blt;
        else if (opCode == _blt)
            opCode = _bge;
        else if (opCode == _ble)
            opCode = _bgt;
        else if (opCode == _bgt)
            opCode = _ble;
    }

    void SetRegisterOp(std::string&& str,bool Flag = Register::vir)
    {
        auto Regop = std::make_shared<Register>(str, Flag);
        opsVec.push_back(Regop);
    }
    void SetVirRegister()  {
        SetRegisterOp("%." + std::to_string(Register::VirtualReg));
    }
    void SetRealRegister(std::string&& str) {
        SetRegisterOp(std::move(str),Register::real);   
    } 
    void SetstackOffsetOp(std::string &&str) {   // prolo  epilo
        auto stackOff = std::make_shared<stackOffset>(str);
        opsVec.push_back(stackOff);
    }
    void SetImmOp(Value *val)
    {
        std::shared_ptr<Imm> Immop = Imm::GetImm(val->as<ConstantData>());
        opsVec.push_back(Immop);
    }
    
    void SetAddrOp(std::string hi_lo,Value* val)
    {
        std::string s1(hi_lo+"(" + val->GetName() + ")");
        std::shared_ptr<RISCVAddrOp> addrOp = std::make_shared<RISCVAddrOp> (s1);
        opsVec.push_back(addrOp);
    }
    
    void SetAddrOp(Value* val)  // la reg, .G.a
    {
        std::string s1(val->GetName());
        std::shared_ptr<RISCVAddrOp> addrOp = std::make_shared<RISCVAddrOp> (s1);
        opsVec.push_back(addrOp);
    }

    void deleteOp(int index)  { opsVec.erase(opsVec.begin() + index); }

    void push_back(op Op) { opsVec.push_back(Op); }
    std::vector<op> &getOpsVec() { return opsVec; }
    std::vector<Instsptr> &getInsts() { return Insts; }
    op getOpreand(int i)
    {
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

    void setThreeRigs(op op1, op op2) // addw
    {
        SetRegisterOp("%." + std::to_string(Register::VirtualReg));
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
};


class RISCVBlock:public RISCVOp,public List<RISCVBlock, RISCVInst>, public Node<RISCVFunction, RISCVBlock>
{
    BasicBlock* cur_bb;
    std::set<Register*> LiveUse;
    std::set<Register*> LiveDef;
    std::vector<BasicBlock*> succBlocks;
    static int counter;
public:
    RISCVBlock(BasicBlock* bb,std::string name)
              :cur_bb(bb) , RISCVOp(name), LiveUse{}, LiveDef{} {    }
    ~RISCVBlock() = default;
    static std::string getCounter();
    std::vector<BasicBlock*> getSuccBlocks();
    std::set<Register*>& getLiveUse()  {  return LiveUse; }
    std::set<Register*>& getLiveDef()  {  return LiveDef; }
    BasicBlock*& getIRbb() { return cur_bb; }
};

// 栈帧的大小  都多余了
// class FrameObject:public RISCVOp
// {
//     std::vector<RISCVInst*> StoreInsts;
// public:
//     std::vector<RISCVInst*>& getStoreInsts()
//     {
//         return StoreInsts;
//     }    

//     void RecordStackMalloc(RISCVInst* inst)
//     {
//         StoreInsts.push_back(inst);
//     }
// };

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
    using offset = size_t;
    std::map<AllocaInst*,offset> AllocaOffsetRecord;

    std::map<RISCVInst*,AllocaInst*> StoreInsts;
    std::vector<RISCVInst*> LoadInsts;
    std::vector<AllocaInst*> AllocaInsts;
    
    std::list<RISCVBlock*> recordBBs;  // 记录顺序
    std::map<size_t,size_t> oldBBindexTonew;
public:
    offset arroffset = 16;
    offset defaultSize = 16;
private:
    // 处理数组，局部与全局的处理
    std::map<Instruction*,offset> recordGepOffset;
    std::map<Value*,Value*> GepGloblToLocal;
    // 全局变量，除了数组
    std::vector<Instruction*> globlValRecord; 

    std::vector<std::pair<Instruction*,std::pair<BasicBlock*,BasicBlock*>>> recordBrInstSuccBBs;
    std::vector<RISCVInst*> LabelInsts;

    std::map<Value*,offset> LocalArrToOffset;
public:
    RISCVFunction(Function* _func,std::string name)
                :func(_func),RISCVOp(name)     {   }
    std::vector<RISCVInst*>&  getLabelInsts() { return LabelInsts; }
    std::vector<std::pair<Instruction*,std::pair<BasicBlock*,BasicBlock*>>>& getBrInstSuccBBs() { return recordBrInstSuccBBs; }
    std::list<RISCVBlock*>& getRecordBBs()  { return recordBBs; }
    std::map<size_t,size_t>& OldToNewIndex() { return oldBBindexTonew;}
    std::map<Instruction*,offset>& getRecordGepOffset() { return recordGepOffset; }
    std::vector<AllocaInst*>& getAllocas()  { return AllocaInsts;  }
    std::map<Value*,Value*>&getGepGloblToLocal()  { return GepGloblToLocal;}
    std::vector<Instruction*>& getGloblValRecord() { return globlValRecord; }
    std::vector<RISCVInst*>& getLoadInsts()  {   return LoadInsts;    }
    std::map<RISCVInst*,AllocaInst*>& getStoreInsts() {   return StoreInsts;    }    
    std::map<AllocaInst*,lastStoreInstPtr>& getStoreRecord() {   return StackStoreRecord;   }
    std::map<matchLoadInstPtr,AllocaInst*>& getLoadRecord() {   return StackLoadRecord;   }
    std::map<AllocaInst*,size_t>& getAOffsetRecord() { return AllocaOffsetRecord; }
    std::map<Value*,offset>& getLocalArrToOffset() { return LocalArrToOffset;}
    void RecordStackMalloc(RISCVInst* inst,AllocaInst* alloca)
    {
        StoreInsts.emplace( std::make_pair(inst,alloca) );
    }

    void getCurFuncArrStack(RISCVInst*& ,Value* val,Value* alloc);

    void setPrologue(std::shared_ptr<RISCVPrologue>& it ) { prologue = it;}
    void setEpilogue(std::shared_ptr<RISCVEpilogue>& it ) { epilogue = it;}

    std::shared_ptr<RISCVPrologue>& getPrologue() { return prologue;}
    std::shared_ptr<RISCVEpilogue>& getEpilogue() { return epilogue;}
            
    ~RISCVFunction() = default;
};
