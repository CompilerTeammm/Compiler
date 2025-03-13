#include "../include/Backend/RISCVAsmPrinter.hpp"

SegmentType __oldtype =TEXT;
SegmentType* oldtype =&__oldtype;
//真change了吗?
SegmentType ChangeSegmentType(SegmentType newtype){
    return newtype;
}

void PrintSegmentType(SegmentType newtype,SegmentType* oldtype){
    if(newtype=*oldtype){
        return;
    }else{
        *oldtype=ChangeSegmentType(newtype);
        if(newtype==TEXT){
            std::cout<<"    .text"<<std::endl;
        }else if(newtype==DATA){
            std::cout<<"    .data"<<std::endl;
        }else if(newtype==BSS){
            std::cout<<"    .bss"<<std::endl;
        }else if(newtype==RODATA){
            std::cout<<"    .section    .rodata"<<std::endl;
        }else{
            std::cout<<"ERROR: Illegal SegmentType"<<std::endl;
        }
    }
}
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
