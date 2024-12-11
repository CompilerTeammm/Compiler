#include "../include/Backend/RISCVAsmPrinter.hpp"

//AsmPrinter 初始化一个实例 
//LLVM 的 Module 类代表了一个编译单元（Compilation Unit）
RISCVAsmPrinter::RISCVAsmPrinter(std::string filename,Moudle* unit, RISCVLoweringContext& ctx): filename(filename){
    dataSegment *data=new dataSegment(unit,ctx);
    this->data=data;
}

//dataSegment 初始化一个实例 生成全局变量列表,num_lable为用于跟踪生成的标签lable的数量?
dataSegment::dataSegment(Moudle* module, RISCVLoweringContext& ctx){
    //GenerateGloblvarList(module, ctx);
   // num_lable=0;
}
