#pragma once
#include "../../include/Backend/RISCVMOperand.hpp"
#include "../../include/Backend/RISCVFrameContext.hpp"
#include "../../include/lib/MyList.hpp"

class RISCVFrame;
class RISCVFunction;
class RISCVBasicBlock;

class RISCVMIR;

class RISCVMIR : public Node<RISCVBasicBlock, RISCVMIR>
{
  RISCVMOperand *def = nullptr;          // 指向目标操作数
  std::vector<RISCVMOperand *> operands; // 数组存储目标操作数

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
  // 构造函数初始化
  RISCVMIR(RISCVISA _isa) : opcode(_isa) {};

  // 操作数管理
  RISCVMOperand *&GetDef();
  RISCVMOperand *&GetOperand(int);
  const int GetOperandSize() { return operands.size(); }
  void SetDef(RISCVMOperand *);
  void SetOperand(int, RISCVMOperand *);
  void AddOperand(RISCVMOperand *);

  // 操作码管理
  void SetMopcode(RISCVISA);
  inline RISCVISA &GetOpcode() { return opcode; };

  // 指令类型判断
  bool isArithmetic()
  {
    return (EndArithmetic > opcode && opcode > BeginArithmetic) | (EndFloatArithmetic > opcode && opcode > BeginFloatArithmetic);
  }
  // 调试输出
  void printfull();
};

struct Terminator
{
  double prob2true = 0.5;
  RISCVMIR *branchinst = nullptr;
  bool implictly = false;
  RISCVBasicBlock *trueblock = nullptr;
  RISCVBasicBlock *falseblock = nullptr;

  inline void SetProb(double _p) { prob2true = _p; };       // 设置分支频率，用于优化
  inline bool isUncond() { return falseblock == nullptr; }; // 判断是不是无条件跳转
  inline bool isRet() { return branchinst == nullptr; };    // 判断是不是ret指令
  void RotateCondition();                                   // 分支概率优化
  void makeFallthrough(RISCVBasicBlock *cand);              // 强制顺序执行，以上跳转全pass
};

class RISCVBasicBlock : public NamedMOperand, public List<RISCVBasicBlock, RISCVMIR>, public Node<RISCVFunction, RISCVBasicBlock>
{
  Terminator term;

public:
  RISCVBasicBlock(std::string);
  static RISCVBasicBlock *CreateRISCVBasicBlock();
  Terminator &getTerminator();                             // 获取基本块的终止指令
  void push_before_branch(RISCVMIR *);                     // 基本块终止指令前，插入新指令，比如cmp比较等，寄存器溢出
  void replace_succ(RISCVBasicBlock *, RISCVBasicBlock *); // 后继基本块更新
  void printfull();
  void erase(RISCVMIR *inst);
};

class RISCVFunction : public RISCVGlobalObject, public List<RISCVFunction, RISCVBasicBlock>
{
  Value *func;
  using RISCVframe = std::unique_ptr<RISCVFrame>;
  RISCVframe frame;

  RISCVBasicBlock exit;
  size_t max_param_size = 0;

  std::vector<int> param_need_spill;

public:
  RISCVFunction(Value *);
  RISCVframe &GetFrame();                          // 获取栈帧
  Register *GetUsedGlobalMapping(RISCVMOperand *); // 获取全局变量操作数对应的虚拟寄存器

  std::unordered_map<RISCVMOperand *, VirRegister *> usedGlobals; // 操作码和虚拟寄存器的映射
  RISCVMIR *CreateSpecialUsageMIR(RISCVMOperand *);

  size_t GetMaxParamSize();              // 获取参数空间，——>寄存器传递 or 栈传递
  void SetMaxParamSize(size_t);          // 设置参数最大空间（防止溢出）
  void GenerateParamNeedSpill();         // 标记溢出到栈上的
  std::vector<int> &GetParamNeedSpill(); // 获取溢出到栈上的

  void printfull();

  inline RISCVBasicBlock *GetEntry() { return GetFront(); };
  inline RISCVBasicBlock *GetExit() { return &exit; };
  uint64_t GetUsedPhyRegMask(); // 查询哪些物理寄存器被使用

  // 获取操作数
  std::unordered_map<VirRegister *, RISCVMOperand *> specialusage_remapping;
  inline RISCVMOperand *GetSpecialUsageMOperand(VirRegister *vreg)
  {
    if (specialusage_remapping.find(vreg) != specialusage_remapping.end())
      return specialusage_remapping[vreg];
    return nullptr;
  };
};

// 函数栈帧的创建
class RISCVFrame
{
public:
  RISCVFrame(RISCVFunction *);                            // 初始化栈帧对象
  StackRegister *spill(VirRegister *);                    // 虚拟寄存器 溢出到 栈帧，用栈寄存器代替
  RISCVMIR *spill(PhyRegister *);                         // 物理寄存器 一出道 栈帧，返回对应的存储指令
  RISCVMIR *load_to_preg(StackRegister *, PhyRegister *); // 从栈帧（Stack Frame）加载数据到物理寄存器（PhyRegister）

  void
  GenerateFrame();          // 计算 offset ,并确保是 16字节对齐
  void GenerateFrameHead(); // SP 和 BP 指针 ，栈首
  void GenerateFrameTail(); // 栈尾
  void AddCantBeSpill(RISCVMOperand *);
  bool CantBeSpill(RISCVMOperand *);

  std::vector<std::unique_ptr<RISCVFrameObject>> &GetFrameObjs();

private:
  std::vector<RISCVMOperand *> cantbespill;
  RISCVFunction *parent;
  size_t frame_size;
  std::unordered_map<VirRegister *, StackRegister *> spillmap;
  std::vector<std::unique_ptr<RISCVFrameObject>> frameobjs;
};
