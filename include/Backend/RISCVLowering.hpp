#include "BackEndPass.hpp"
#include "../../include/lib/Module.hpp"

// ���� LLVM IR �� RISCV ת������
class RISCVModuleLowing : BackEndPass<Module>
{
  void operator()(RISCVFunction *); // ��ʼ��������������
  RISCVFunction *&GetCurFunction(); // ��ȡ��ǰ���ڴ���ĺ���
};