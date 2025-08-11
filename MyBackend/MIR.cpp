#include "../include/MyBackend/MIR.hpp"
#include "../include/IR/Analysis/Dominant.hpp"
#include "../include/MyBackend/RISCVContext.hpp"
#include "../include/MyBackend/RISCVType.hpp"
#include <memory>
#include <string>
#include <sstream>

Imm::Imm(ConstantData* _data) :type(TransType(_data->GetType())),
                                data(_data) { ImmInit(); }
ConstantData* Imm::getData() { return data; }

void Imm::ImmInit() {
    if(type == riscv_32int) {
        int val = dynamic_cast<ConstIRInt*> (data)->GetVal();
        setName(std::to_string(val));
    } else if(type == riscv_32float) {
        float fval = dynamic_cast<ConstIRFloat*> (data)->GetVal();
        setName(floatToString(fval));
    }
}
std::shared_ptr<Imm> Imm::GetImm(ConstantData* _data)
{
    static std::map<ConstantData*,std::shared_ptr<Imm>> Immpool;
    if(Immpool.find(_data) == Immpool.end())
        Immpool[_data] = std::make_shared<Imm>(_data);
    return Immpool[_data];
}

Register::Register(std::string _name) :RISCVOp(_name) {  }

int VirRegister::VirtualRegNum = 0;
// have a problem
void VirRegister::reWriteRegWithReal(RealRegister* _real)
{
    auto op = _real->getRegop();
    setName(RealRegister::realRegToString(op));
}

RealRegister::realReg RealRegister::getRegop()  
{ 
    return realRegop;
}
// global is only one
RealRegister* RealRegister::GetRealReg(RealRegister::realReg _Regop)
{
    static std::unordered_map<realReg,RealRegister*> realRegMap;
    auto it = realRegMap.find(_Regop);
    if (it == realRegMap.end()) 
        it = realRegMap.emplace(_Regop,new RealRegister(_Regop)).first;  
    return it->second;
}

std::string RealRegister::realRegToString(realReg reg)
{
    
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


std::string  RISCVInst::ISAtoAsm()
{
    if(opCode == _ret) {  return "ret"; }
    if(opCode == _li) { return "li"; }
    if(opCode == _addi) {return "addi";}
    if(opCode == _ld) { return "ld";}
    if(opCode == _sd) { return "sd";}
    if(opCode == _sw) { return "sw";}
    if(opCode == _lw) { return "lw";}
    if(opCode == _fmv_w_x ) { return "fmv.w.x"; }
    if(opCode == _fsw ) { return "fsw"; }
    if(opCode == _j) { return "j"; }
    if(opCode == _ble) { return "ble";}
    if(opCode == _bge) { return "bge"; }
    if(opCode == _bgt) { return "bgt"; }
    if(opCode == _blt) { return "blt"; }
    if(opCode == _bne) { return "bne"; }
    if(opCode == _beq) { return "beq"; }
    if(opCode == _addw ) { return "addw"; }
    if(opCode == _subw) { return "subw"; }
    if(opCode == _mulw) { return "mulw"; }
    if(opCode == _divw) { return "divw"; }
    if(opCode == _remw) { return "remw"; }
    if(opCode == _flw) { return "flw"; }
    if(opCode == _fadd_s) { return "fadd.s";}
    if(opCode == _fsub_s) { return "fsub.s"; }
    if(opCode == _fmul_s) { return "fmul.s"; }
    if(opCode == _fdiv_s) { return "fdiv.s"; }
    if(opCode == _fmv_s) { return "fmv.s"; }
    if(opCode == _fcvt_w_s) {return "fcvt.w.s";}
    if(opCode == _fcvt_s_w) { return "fcvt.s.w";}
    if(opCode == _mv) { return "mv"; }
    if(opCode == _flt_s) { return "flt.s"; } 
    if(opCode == _beq) { return "beq"; }
    if(opCode == _fle_s) { return "fle.s"; }
    if(opCode == _feq_s) { return "feq.s"; }
    if(opCode == _seqz) { return "seqz"; }
    if(opCode == _divw )  {return "divw";}
    if(opCode == _call)  { return "call"; }
    if(opCode == _lui) { return "lui"; }
    if(opCode == _andi )  { return "andi"; }
    if(opCode == _slli)  { return "slli"; }
    if(opCode == _add)  { return "add"; }
    if(opCode == _seqz) { return "seqz";}
    if(opCode == _xor) { return "xor";}
    if(opCode == _mul) { return "mul"; }
    if(opCode == _la) { return "la"; }
 
    return nullptr;
}

int RISCVBlock::counter = 0;
std::string RISCVBlock:: getCounter() { 
    
    return std::to_string(counter++); 
}

std::vector<BasicBlock*> RISCVBlock::getSuccBlocks()
{
    succBlocks.clear();
    auto it = cur_bb->GetParent();
    DominantTree tree(cur_bb->GetParent());
    tree.BuildDominantTree();
    auto succBlocks = tree.getSuccBBs(cur_bb);
    return succBlocks;
}

//  有一个函数可以 getLocalArrToOffset
//  RInst--> 后端语句    val---> size     alloca  ---> allocaInst*
void RISCVFunction::getCurFuncArrStack(RISCVInst*& RInst,Value* val,Value* alloca)
{
    arroffset += std::stoi(val->GetName());
    RInst->SetstackOffsetOp("-"+std::to_string(arroffset));
    LocalArrToOffset[alloca] = arroffset;
}
