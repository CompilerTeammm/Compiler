#include "../include/Backend/RISCVContext.hpp"
#include "../include/Backend/RISCVAsmPrinter.hpp"

void RISCVLoweringContext::insert_val2mop(Value *val, RISCVMOperand *mop)
{
  val2mop.insert(std::make_pair(val, mop)); // 建立一个新的value和机器操作数
}