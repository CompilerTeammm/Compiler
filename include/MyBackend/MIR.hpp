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
// 地址操作符
class RISCVAddrOp:public RISCVOp 
{
public:
    RISCVAddrOp(std::string name) :RISCVOp(name) { }
};

class RealRegister;
class VirRegister;
class Register:public RISCVOp
{
public:
    Register(std::string _name);
    int Floatflag; 
    bool IsFloatFlag() { return Floatflag == 1; }   // FloatReg or  IntReg
};

class VirRegister:public Register
{
public:
    static int VirtualRegNum;
    VirRegister():Register("%"+ std::to_string(VirtualRegNum)) 
                  {   VirtualRegNum++;   }
    void setFloatflag() { Floatflag = 1; }
    void reWriteRegWithReal(RealRegister* );
};

class RealRegister:public Register
{
public:
    enum realReg{
        // x0=zero,x1=ra,x2=sp,x3=gp,x4=tp,x5=t0,x6=t1,x7=t2,
        // x8=s0,x9=s1,x10=a0,x11=a1,x12=a2,x13=a3,x14=a4,x15=a5,
        // x16=a6,x17=a7,x18=s2,x19=s3,x20=s4,x21=s5,x22=s6,x23=s7,
        // x24=s8,x25=s9,x26=s10,x27=s11,x28=t3,x29=t4,x30=t5,x31=t6,
        // INT  ABI name
        zero,ra,sp,gp,tp,t0,t1,s0,
        // caller_saved
        t2,t3,t4,t5,t6,a0,a1,a2,a3,a4,a5,a6,a7,
        // callee_saved
        s1,s2,s3,s4,s5,s6,s7,s8,s9,s10,s11,
        
        // FLOAT
        // caller_saved
        fa0,fa1,fa2,fa3,fa4,fa5,fa6,fa7,
        // callee_saved
        fs0,fs1,fs2,fs3,fs4,fs5,fs6,fs7,fs8,fs9,fs10,fs11,
         _NULL,
    };

    bool isCallerSaved() {

    } 
    bool isCalleeSaved() {
        
    }
    realReg realRegop;
    static std::string realRegToString(realReg reg);
    static RealRegister* GetRealReg(realReg);
    realReg getRegop();
    RealRegister(realReg _Regop):Register(realRegToString(_Regop)),realRegop(_Regop) {}
    RealRegister(std::string name):Register(name) {}
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

    // Virtual
    void SetVirRegister()  {
        auto Regop = std::make_shared<VirRegister>();
        opsVec.push_back(Regop);
    }

    void SetVirFloatRegister()  {
        auto Regop = std::make_shared<VirRegister>();
        Regop->setFloatflag();
        opsVec.push_back(Regop);
    }
    // Real
    void SetRealRegister(std::string&& str) {
        auto Regop = std::make_shared<RealRegister>(str);
        opsVec.push_back(Regop); 
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
    void setStoreStackS0Op(size_t offset)
    {
       opsVec.push_back(std::make_shared<stackOffset>("-" + std::to_string(offset) + "(s0)"));
    }


    void setThreeRegs(op op1, op op2) // addw
    {
        SetVirRegister();
        opsVec.push_back(op1);
        opsVec.push_back(op2);
    }

    void setThreeFRegs(op op1, op op2) // addw
    {
        SetVirFloatRegister();
        opsVec.push_back(op1);
        opsVec.push_back(op2);
    }

    void setRetOp(Value* val)  // ret
    {
        SetRealRegister("a0");
        SetImmOp(val);
    }
    void setFRetOp(Value* val)  // ret
    {
        SetRealRegister("fa0");
        SetImmOp(val);
    }
    void setRetOp(op val)
    {
        SetRealRegister("a0");
        opsVec.push_back(val);
    }
    void setFRetOp(op val)
    {
        SetRealRegister("fa0");
        opsVec.push_back(val);
    }

    void setVirLIOp(Value* val) // li
    {
        SetVirRegister();
        SetImmOp(val);
    }
    void setRealLIOp(Value* val) // li
    {
        SetRealRegister("t0");
        SetImmOp(val);
    }

    void setMVOp(RISCVInst* Inst)
    {
        SetVirRegister();
        auto reg = Inst->getOpreand(0);
        opsVec.push_back(reg);
    }

    void setOpWInstFOpread(RISCVInst* Inst) 
    {
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
    std::set<Register*>& getLiveUse()  {  return LiveUse; }   // LiveUse or LiveDef 都是VirReg
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
    typedef std::vector<Instptr> EpilogueInsts;
    EpilogueInsts epiloInsts;
public:
    EpilogueInsts& getInstsVec()
    {
        return epiloInsts;
    }
};

// Function include BBs Name 
// And func produce the frame 
class RISCVFunction:public RISCVOp, public List<RISCVFunction, RISCVBlock>
{
    Function* func;
    RISCVBlock* exit;   
    std::shared_ptr<RISCVPrologue> prologue;
    std::shared_ptr<RISCVEpilogue> epilogue;

    using matchLoadRInst = RISCVInst*;
    std::map<matchLoadRInst,AllocaInst*> StackLoadRecord;
    using offset = size_t;
    std::map<AllocaInst*,offset> AllocaOffsetRecord;

    using matchStoreRInst = RISCVInst*;
    std::map<matchStoreRInst,AllocaInst*> StackStoreRecord;
    std::vector<RISCVInst*> LoadInsts;
    std::vector<AllocaInst*> AllocaInsts;

    // 处理数组，局部与全局的处理
    std::map<Instruction*,offset> recordGepOffset;
    std::map<Value*,Value*> GepGloblToLocal;

    std::map<Value*,offset> LocalArrToOffset;
    // 全局变量，除了数组
    std::vector<Instruction*> globlValRecord; 

    
    std::list<RISCVBlock*> recordBBs;  // 记录顺序
    std::map<size_t,size_t> oldBBindexTonew;
    
    std::vector<std::pair<Instruction*,std::pair<BasicBlock*,BasicBlock*>>> recordBrInstSuccBBs;
    std::vector<RISCVInst*> LabelInsts;
public:
    offset arroffset = 16;    // arr space =  arroffset - defaulSize
    offset defaultSize = 16;  // 存储 ra,sp 的栈帧space
public:
    RISCVFunction(Function* _func,std::string name)
                :func(_func),RISCVOp(name)     {   }
    
    // adjust bbs 
    std::vector<RISCVInst*>&  getLabelInsts() { return LabelInsts; }
    std::vector<std::pair<Instruction*,std::pair<BasicBlock*,BasicBlock*>>>& getBrInstSuccBBs() { return recordBrInstSuccBBs; }
    std::list<RISCVBlock*>& getRecordBBs()  { return recordBBs; }
    std::map<size_t,size_t>& OldToNewIndex() { return oldBBindexTonew;}
    

    // local Int float deal
    //std::map<AllocaInst*,lastStoreRInst>& getStoreRecord() {   return StackStoreRecord;   }
    std::map<matchLoadRInst,AllocaInst*>& getLoadRecord() {   return StackLoadRecord;   }
    std::map<matchStoreRInst,AllocaInst*>& getStoreRecord() {   return StackStoreRecord;    }   
    std::vector<RISCVInst*>& getLoadInsts()  {   return LoadInsts;    } 
    std::vector<AllocaInst*>& getAllocas()  { return AllocaInsts;  }
    void RecordStackMalloc(RISCVInst* inst,AllocaInst* alloca) {
       StackStoreRecord.emplace( std::make_pair(inst,alloca) );
    }

    // 处理arr stack offset     
    // gloabl val
    std::map<AllocaInst*,size_t>& getAOffsetRecord() { return AllocaOffsetRecord; }
    std::map<Value*,offset>& getLocalArrToOffset() { return LocalArrToOffset;}
    void getCurFuncArrStack(RISCVInst*& ,Value* val,Value* alloc);
    std::map<Instruction*,offset>& getRecordGepOffset() { return recordGepOffset; }
    std::map<Value*,Value*>&getGepGloblToLocal()  { return GepGloblToLocal;}
    std::vector<Instruction*>& getGloblValRecord() { return globlValRecord; }


    // prologue and epilogue
    void setPrologue(std::shared_ptr<RISCVPrologue>& it ) { prologue = it;}
    void setEpilogue(std::shared_ptr<RISCVEpilogue>& it ) { epilogue = it;}
    std::shared_ptr<RISCVPrologue>& getPrologue() { return prologue;}
    std::shared_ptr<RISCVEpilogue>& getEpilogue() { return epilogue;}
            
    ~RISCVFunction() = default;

// solve the paramNums problem
private:
    // callee
    size_t paramNum;
    std::vector<RISCVInst*> spilledParam;
    std::vector<IR_DataType> spilledParamType;
    std::vector<RISCVInst*> spilledParamLoadInst;

    // caller
    int MallocStackForparam;
public:
    size_t& getparamNum() { return paramNum; }
    std::vector<RISCVInst*>&  getSpilledParam()  { return spilledParam;}
    // need to save the last value Type
    std::vector<IR_DataType>& getSpilledParamType()  { return spilledParamType; }
    std::vector<RISCVInst*>& getSpilledParamLoadInst() { return spilledParamLoadInst; }
    
    // caller 
    int& getNeadStackForparam()  { return MallocStackForparam;} 
};
