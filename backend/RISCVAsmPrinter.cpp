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

void RISCVAsmPrinter::SetTextSegment(textSegment* _text){
    text=_text;
}
dataSegment* &RISCVAsmPrinter::GetData{
    return data;
}

void RISCVAsmPrinter::set_use_cachelookup(bool condi){
    use_cachelookup=condi;
}
void RISCVAsmPrinter::set_use_cachelookup4(bool condi){
    use_cachelookup4=condi;
}

void RISCVAsmPrinter::PrintAsmGlobal(){
    std::cout << "    .file  \"" << filename << "\"" << std::endl;
    std::cout << "    .attribute arch, \"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0_zba1p0_zbb1p0\"" << std::endl;
    std::cout << "    .attribute unaligned_access, 0" << std::endl; 
    std::cout << "    .attribute stack_align, 16" << std::endl;
    std::cout << "    .text" << std::endl;
    this->data->PrintDataSegment_Globval();    
}
//这样的格式可以将.hpp代码内容直接包含进来
// void RISCVAsmPrinter::PrintCacheLookUp(){
//     static const char* cachelookuplib= 
//     #include "../include/RISCVSupport?cachelib.hpp"
//     ;
//     std::cout<<cachelookuplib;
// }
// void RISCVAsmPrinter::PrintCacheLookUp4(){
//     static const char* cachelookuplib4=
//     #include "../include/RISCVSupport/cachelib4.hpp"
//     ;
//     std::cout<<cachelookuplib4;
// }
// void RISCVAsmPrinter::PrintParallelLib(){
//     static const char* buildinlib=
//     #include "../include/RISCVSupport/parallel.hpp"
//     ;
//     std::cout<<buildinlib;
// }

void RISCVAsmPrinter::PrintAsm(){
    this->PrintAsmGlobal();
    this->text->PrintTextSegment();
    this->data->PrintDataSegment_Tempvar();
    // if(Singleton<Enable_Parallel>().flag==true){
    //     this->PrintParallelLib();
    // }
    if(this->use_cachelookup==true){
        this->PrintCacheLookUp();
    }
    if(this->use_cachelookup4==true){
        this->PrintCacheLookUp4();
    }
}
//dataSegment 初始化一个实例 生成全局变量列表,num_lable为用于跟踪生成的标签lable的数量?
dataSegment::dataSegment(Moudle* module, RISCVLoweringContext& ctx){
    GenerateGloblvarList(module, ctx);
    num_lable=0;
}
void dataSegment::GenerateGloblvarList(Module* moudle,RISCVLoweringContext& ctx){
    for(auto& data:moudle->GetGlobalVariable()){
        globlvar* gvar=new globlvar(data.get());
        ctx.insert_val2mop(dynamic_cast<Value*>(data.get()),gvar);
        globlvar_list.push_back(gvar);
    }
}
//textSegment
textSegment::textSegment(RISCVLoweringContext& ctx){
    GenerateFuncList(ctx);
}
void textSegment::GenerateFuncList(RISCVLoweringContext& ctx){
    for(auto& function : ctx.GetFunctions()){
        functionSegment* funcSeg=new functionSegment(function.get());
        function_list.push_back(funcSeg);
    }
}
void textSegment::PrintTextSegment(){
    PrintSegmentType(TEXT,oldtype);
    for(auto& functionSegment: function_list){
        functionSegment->PrintFuncSegment();
    }
}
//functionSegment
functionSegment::functionSegment(RISCVFunction* function):func(function){
    align=1;
    name=function->GetName();//MIR
}
void functionSegment::PrintFuncSegment(){
    std::cout << "    .align  " << align << std::endl;
    std::cout << "    .globl  " << name << std::endl;
    std::cout << "    .type  " << name << ", @" << ty << std::endl;
    func->printfull();//MIR
    if(size==-1){
        std::cout<<"    .size"<<name<<", "<<".-"<<name<<std::endl;
    }    
}