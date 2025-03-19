#include "../include/Backend/RISCVMIR.hpp"

std::unique_ptr<RISCVFrame> &RISCVFunction::GetFrame()
{
  return frame;
}