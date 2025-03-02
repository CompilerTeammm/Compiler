
#include "../../include/Backend/RISCVMIR.hpp"
class RISCVFunction; // RISCV机器函数（机器指令、BB信息）

class RISCVLoweringContext
{
private:
  std::map<Value *, RISCVMOperand *> val2mop;

  using MFuncPtr = std::unique_ptr<RISCVFunction>;
  std::vector<MFuncPtr> functions; // functions 管理 所有RISCVFunction对象的生命周期

  RISCVFunction *cur_func;
  RISCVBasicBlock *cur_mbb;

  // @note Value ——> RISCVMOperand
  // case1：user（用户指令）：return 虚拟寄存器（vreg）
  // case1.1：Alloca(分配内存)：return RISCVFrameObject，并将其插入到当前函数中
  // case2：bb（基本块）：return mbb
  // case3：常量：return imm
  // case4：函数：return machine function
  RISCVMOperand *Create(Value *);
  // @warning 实现多态性，返回一个基类指针，指向派生类
public:
  void operator(RISCVMIR *);
  void operator()(RISCVBasicBlock *);
  void operator()(RISCVFunction *);
  void insert_val2mop(Value *, RISCVMOperand *);
  void change_mapping(RISCVMOperand *, RISCVMOperand *);

  VirRegister *createVReg(RISCVType);
  std::vector<MFuncPtr> &GetFunctions();

  // 查找
  RISCVMOperand *mapping(Value *);
  Value *GetValue(RISCVMOperand *);

  // current
  RISCVFunction *&GetCurFunction();
  RISCVBasicBlock *&GetCurBasicBlock();

  void print();
};