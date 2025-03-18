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
void RISCVAsmPrinter::PrintCacheLookUp(){
    static const char* cachelookuplib= 
    #include "../include/RISCVSupport/cachelib.hpp"
    ;
    std::cout<<cachelookuplib;
}
void RISCVAsmPrinter::PrintCacheLookUp4(){
    static const char* cachelookuplib4=
    #include "../include/RISCVSupport/cachelib4.hpp"
    ;
    std::cout<<cachelookuplib4;
}
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
void dataSegment::GenerateTempvarList(RISCVLoweringContext& ctx){
    return;//直接返回不执行？
    for(auto& function:ctx.GetFunctions()){
        for(auto block:*function){
            for(mylist<RISCVBasicBlock,RISCVMIR>::iterator it=block->begin();it!=block->end();++it){
                RISCVMIR* machineinst=*it;
                if(machineinst->GetOperandSize()=0){
                    continue;//过滤掉没有操作数的指令
                }
                //生成放在只读数据段内容，浮点常量
                for(int i=0;i<machineinst->GetOperandSize();i++){
                    if(IMM* used=dynamic_cast<Imm*>(machineinst->GetOperand(i))){
                        if(auto constfloat=dynamic_cast<ConstIRFloat*>(used->Getdata())){
                            tempvar* tempfloat=nullptr;
                            for(int i=0;i<tempvar_list.size();i++){
                                if(tempvar_list[i]->GetInit()==constfloat->GetVal()){
                                    tempfloat=tempvar_list[i];
                                    break;
                                }
                            }
                            if(tempfloat==nullptr){
                                tempfloat=new tempvar(num_lable,constfloat->GetVal());
                                this->num_lable++;
                                tempvar_list.push_back(tempfloat);
                            }
                            machineinst->SetOperand(i,tempfloat);//用tempfloat替换机器指令中的立即数
                        }
                    }
                }
            }
        }
    }
}
std::vector<tempvar*> dataSegment::get_tempvar_list(){
    return tempvar_list;
}
void dataSegment::Change_LoadConstFloat(RISCVMIR* inst,tempvar*tempfloat,mylist<RISCVBasicBlock,RISCVMIR>::iterator it,Imm* used){
    if(inst->GetOpcode()==RISCVMIR::call){
        return;
    }
    std::string opcode(magic_enum::enum_name(inst->GetOpcode()));
    RISCVBasicBlock* block=inst->GetParent();
    std::unique_ptr<RISCVFrame>& frame=block->GetParent()->GetFrame();
    
    std::string name=tempfloat->Getname();
    VirRegister* lui_rd=new VirRegister(RISCVType::riscv_ptr);
    LARegister* lui_rs==new LARegister(RISCVType::riscv_ptr,name);
    VirRegister* flw_rd=new VirRegister(RISCVType::riscv_float32);
    LARegister* flw_rs=new LARegister(RISCVType::riscv_ptr,name,lui_rd);

    RISCVMIR* lui=new RISCVMIR(RISCVMIR::RISCVISA::_lui);
    lui->SetDef(lui_rd);
    lui->AddOperand(lui_rs);
    frame->AddCantBeSpill(lui_rd);

    RISCV* flw=new RISCVMIR(RISCVMIR::RISCVISA::_flw);
    flw->SetDef(flw_rd);
    flw->AddOperand(flw_rs);
    it.insert_before(lui);
    it.insert_before(flw);

    for(int i=0;i<inst->GetOperandSize();i++){
        while(inst->GetOperand(i)==used){
            inst->SetOperand(i,flw_rd);
        }
    }
}
//print
void dataSegment::PrintDataSegment_Globval(){
    for(auto& gvar:globlvar_list){
        gvar->PrintGloblvar();
    }
}
void dataSegment::PrintDataSegment_Tempvar(){
    for(auto& gvar:tempvar_list){
        gvar->PrintTempvar();
    }
}

void dataSegment::LegalizeGloablVar(RISCVLoweringContext& ctx){
    std::map<globlvar*,VirRegister*> attached_normal;
    std::map<globlvar*,VirRegister*> attached_mem;
    RISCVFunction* cur_func=ctx.GetCurFunction();
    for(auto block:*cur_func){
        attached_normal.clean();
        attached_mem.clean();
        for(){
            
        }
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