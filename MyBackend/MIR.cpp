#include "../include/MyBackend/MIR.hpp"

int Register::VirtualReg = 0;

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
    if(opCode == _bqe) { return "bqe"; }
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

    return nullptr;
}