#include "../../include/Backend/RISCVMOperand.hpp"

class RISCVMIR : public list_node<RISCVBasicBlock, RISCVMIR>
{
  RISCVMOperand *def = nullptr;
  std::vector<RISCVMOperand *> operands;

public:
  enum RISCVISA
  {
    BeginShift,
    // 逻辑左移
    _sll,   // 寄存器
    _slli,  // 立即数
    _sllw,  // 字
    _slliw, // 立即数字
    // 逻辑右移
    _srl,
    _srli,
    _srlw,
    _srliw,
    // 算术右移
    _sra,
    _srai,
    _sraiw,
    _sraw,
    EndShift,

    BeginArithmetic,
    _add,   // add rd, rs1, rs2
    _addi,  // addi rd, rs1, imm    范围为 -2048 到 2047
    _addw,  // addw rd, rs1, rs2    将 rs1 和 rs2 的低 32 位相加，结果符号扩展后存入 rd
    _addiw, // addiw rd, rs1, imm
    _sub,   // sub rd, rs1, rs2
    _subw,  // subw rd, rs1, rs2
    _lui,   // lui rd, imm          加载高位立即数。         ?
    _auipc, // auipc rd, imm        将高位立即数加到当前 PC  ?

    _max,
    _min,

    _mul,
    _mulh,
    _mulhsu,
    _mulhu,
    _mulw,

    _div,
    _divu,
    _rem,
    _remu,

    _divw,
    _remw,
    _remuw,
    EndArithmetic,

    // 逻辑指令
    BeginLogic,
    _xor, // 异或
    _xori,
    _or,
    _ori,
    _and,
    _andi,
    EndLogic,

    BeginComp,
    _seqz, // 判断是否为零
    _snez,
    _slt, // 有符号小于比较
    _slti,
    _sltu, // 无符号小于比较
    _sltiu,
    EndComp,

    BeginBranch,
    _j,   // j offset     无条件跳转到目标地址（PC + offset）
    _beq, // beq rs1, rs2, offset
    _bne,
    _blt,
    _bge,
    _ble,
    _bgt,
    _bltu,
    _bgeu,
    EndBranch,

    BeginJumpAndLink,
    _jalr,
    _jal,
    EndJumpAndLink,

    BeginMem,

    BeginLoadMem,
    _lb,
    _lbu,
    _lh,
    _lhu,
    _lw,
    _ld,
    EndLoadMem,

    BeginStoreMem,
    _sb,
    _sh,
    _sw,
    _sd,
    EndStoreMem,

    EndMem,

    // 类型转换
    BeginConvert,
    _sext_w,
    EndConvert,

    BeginFloat,

    BeginFloatMV,
    _fmv_w_x,
    _fmv_x_w,
    _fmv_s,
    EndFloatMV,

    BeginFloatConvert,
    _fcvt_s_w,
    _fcvt_s_wu,
    _fcvt_w_s,
    _fcvt_wu_s,
    EndFloatConvert,

    BeginFloatMem,

    BeginFloatLoadMem,
    _flw,
    _fld,
    EndFloatLoadMem,

    BeginFloatStoreMem,
    _fsw,
    _fsd,
    EndFloatStoreMem,

    EndFloatMem,

    BeginFloatArithmetic,
    _fadd_s,
    _fsub_s,
    _fmul_s,

    _fmadd_s,
    _fmsub_s,
    _fnmadd_s,
    _fnmsub_s,

    _fdiv_s,
    _fsqrt_s,

    _fsgnj_s,
    _fsgnjn_s,
    _fsgnjx_s,

    _fmin_s,
    _fmax_s,

    _feq_s,
    _flt_s,
    _fle_s,
    _fgt_s,
    _fge_s,

    EndFloatArithmetic,
    EndFloat,

    // 原子操作
    BeginAtomic,
    _amoadd_w_aqrl,
    EndAtomic,

    BeginMIRPseudo,
    mv, // move
    call,
    ret,
    li,

    LoadGlobalAddr,
    LoadImmReg,
    LoadLocalAddr,
    EndMIRPseudo,

    MarkDead,

    BeginPipeline,
    LoadImm12,
    LoadImm32,
    LoadFromStack,
    EndPipeline

  } opcode;
}
}
;
