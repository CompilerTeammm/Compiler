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

int Register::VirtualReg = 0;
Register::Register(std::string _name, bool Flag,int _Fflag) 
                  :RISCVOp(_name), RVflag(Flag),Fflag(_Fflag)
{
    if (Flag)
        VirtualReg++;
}
Register::Register(realReg _Regop,bool Flag,int _Fflag)
               : RVflag(Flag),Fflag(_Fflag),realRegop(_Regop)
{    
    setName(realRegToString(_Regop));
}
Register::realReg Register::getRegop()  
{ 
    if(IsrealRegister()) 
        return realRegop;
    LOG(ERROR, "this is virtual reg!!!");
    return _NULL;
}
// global is only one
Register*Register::GetRealReg(Register::realReg _Regop)
{
    static std::unordered_map<realReg,Register*> realRegMap;
    auto it = realRegMap.find(_Regop);
    if (it == realRegMap.end()) 
        it = realRegMap.emplace(_Regop,new Register(_Regop)).first;  
    return it->second;
}

void Register::reWirteRegWithReal(Register* _real)
{
    setRVflag();
    auto op = _real->getRegop();
    setName(realRegToString(op));
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

void RISCVFunction::getCurFuncArrStack(RISCVInst*& RInst,Value* val,Value* alloca)
{
    arroffset += std::stoi(val->GetName());
    RInst->SetstackOffsetOp("-"+std::to_string(arroffset));
    LocalArrToOffset[alloca] = arroffset;
}
