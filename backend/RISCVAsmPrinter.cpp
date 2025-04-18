#include "../include/Backend/RISCVAsmPrinter.hpp"

SegmentType __oldtype = TEXT;
SegmentType *oldtype = &__oldtype;
SegmentType ChangeSegmentType(SegmentType newtype)
{
    return newtype;
}

void PrintSegmentType(SegmentType newtype, SegmentType *oldtype)
{
    if (newtype = *oldtype)
    {
        return;
    }
    else
    {
        *oldtype = ChangeSegmentType(newtype);
        if (newtype == TEXT)
        {
            std::cout << "    .text" << std::endl;
        }
        else if (newtype == DATA)
        {
            std::cout << "    .data" << std::endl;
        }
        else if (newtype == BSS)
        {
            std::cout << "    .bss" << std::endl;
        }
        else if (newtype == RODATA)
        {
            std::cout << "    .section    .rodata" << std::endl;
        }
        else
        {
            std::cout << "ERROR: Illegal SegmentType" << std::endl;
        }
    }
}
// AsmPrinter 初始化一个实例
// LLVM 的 Module 类代表了一个编译单元（Compilation Unit）
RISCVAsmPrinter::RISCVAsmPrinter(std::string filename, Module *unit, RISCVLoweringContext &ctx) : filename(filename)
{
    dataSegment *data = new dataSegment(unit, ctx);
    this->data = data;
}

void RISCVAsmPrinter::SetTextSegment(textSegment *_text)
{
    text = _text;
}
dataSegment *&RISCVAsmPrinter::GetData()
{
    return data;
}

void RISCVAsmPrinter::set_use_cachelookup(bool condi)
{
    use_cachelookup = condi;
}
void RISCVAsmPrinter::set_use_cachelookup4(bool condi)
{
    use_cachelookup4 = condi;
}

void RISCVAsmPrinter::PrintAsmGlobal()
{
    std::cout << "    .file  \"" << filename << "\"" << std::endl;
    std::cout << "    .attribute arch, \"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0_zba1p0_zbb1p0\"" << std::endl;
    std::cout << "    .attribute unaligned_access, 0" << std::endl;
    std::cout << "    .attribute stack_align, 16" << std::endl;
    std::cout << "    .text" << std::endl;
    this->data->PrintDataSegment_Globval();
}
// 这样的格式可以将.hpp代码内容直接包含进来
/* void RISCVAsmPrinter::PrintCacheLookUp()
{
    static const char *cachelookuplib =
#include "../include/RISCVSupport/cachelib.hpp"
;
    std::cout << cachelookuplib;
} */

void RISCVAsmPrinter::PrintCacheLookUp()
{
    std::ifstream file("../include/RISCVSupport/cachelib.hpp");
    if (file)
    {
        std::cout << file.rdbuf();
    }
    else
    {
        std::cerr << "Error opening cachelib.hpp\n";
    }
}
/* void RISCVAsmPrinter::PrintCacheLookUp4()
{
    static const char *cachelookuplib4 =
#include "../include/RISCVSupport/cachelib4.hpp"
        ;
    std::cout << cachelookuplib4;
} */
void RISCVAsmPrinter::PrintCacheLookUp4()
{
    std::ifstream file("../include/RISCVSupport/cachelib4.hpp");
    if (file)
    {
        std::cout << file.rdbuf();
    }
    else
    {
        std::cerr << "Error opening cachelib4.hpp\n";
    }
}
// void RISCVAsmPrinter::PrintParallelLib(){
//     static const char* buildinlib=
//     #include "../include/RISCVSupport/parallel.hpp"
//     ;
//     std::cout<<buildinlib;
// }

void RISCVAsmPrinter::PrintAsm()
{
    this->PrintAsmGlobal();
    this->text->PrintTextSegment();
    this->data->PrintDataSegment_Tempvar();
    // if(Singleton<Enable_Parallel>().flag==true){
    //     this->PrintParallelLib();
    // }
    if (this->use_cachelookup == true)
    {
        this->PrintCacheLookUp();
    }
    if (this->use_cachelookup4 == true)
    {
        this->PrintCacheLookUp4();
    }
}
// dataSegment 初始化一个实例 生成全局变量列表,num_lable为用于跟踪生成的标签lable的数量?
dataSegment::dataSegment(Module *module, RISCVLoweringContext &ctx)
{
    GenerateGloblvarList(module, ctx);
    num_lable = 0;
}
void dataSegment::GenerateGloblvarList(Module *moudle, RISCVLoweringContext &ctx)
{
    for (auto &data : moudle->GetGlobalVariable())
    {
        globlvar *gvar = new globlvar(data.get());
        ctx.insert_val2mop(dynamic_cast<Value *>(data.get()), gvar);
        globlvar_list.push_back(gvar);
    }
}
void dataSegment::GenerateTempvarList(RISCVLoweringContext &ctx)
{
    return; // 直接返回不执行？
    for (auto &function : ctx.GetFunctions())
    {
        for (auto block : *function)
        {
            for (List<RISCVBasicBlock, RISCVMIR>::iterator it = block->begin(); it != block->end(); ++it)
            {
                RISCVMIR *machineinst = *it;
                if (machineinst->GetOperandSize() == 0)
                {
                    continue; // 过滤掉没有操作数的指令
                }
                // 生成放在只读数据段内容，浮点常量
                for (int i = 0; i < machineinst->GetOperandSize(); i++)
                {
                    if (Imm *used = dynamic_cast<Imm *>(machineinst->GetOperand(i)))
                    {
                        if (auto constfloat = dynamic_cast<ConstIRFloat *>(used->GetData()))
                        {
                            tempvar *tempfloat = nullptr;
                            for (int i = 0; i < tempvar_list.size(); i++)
                            {
                                if (tempvar_list[i]->GetInit() == constfloat->GetVal())
                                {
                                    tempfloat = tempvar_list[i];
                                    break;
                                }
                            }
                            if (tempfloat == nullptr)
                            {
                                tempfloat = new tempvar(num_lable, constfloat->GetVal());
                                this->num_lable++;
                                tempvar_list.push_back(tempfloat);
                            }
                            machineinst->SetOperand(i, tempfloat); // 用tempfloat替换机器指令中的立即数
                        }
                    }
                }
            }
        }
    }
}
std::vector<tempvar *> dataSegment::get_tempvar_list()
{
    return tempvar_list;
}
// 在 RISC-V 架构中，浮点数不能直接通过立即数方式加载到寄存器，而是需要先加载地址，再从内存加载数据。
// 将一个 tempvar* 类型的浮点立即数（tempfloat）转换为从数据段加载的形式，并插入 lui 和 flw 指令，使原本使用该立即数的指令改为从内存加载。
// inst 应该是当前操作的RISCVMIR指令 tempfloat为存储浮点立即数的临时变量
// it 当前基本块的指令迭代器，用于在当前位置插入lui flw指令
// used 原来inst指令中用于存储该浮点立即数的Imm立即数
void dataSegment::Change_LoadConstFloat(RISCVMIR *inst, tempvar *tempfloat, List<RISCVBasicBlock, RISCVMIR>::iterator it, Imm *used)
{
    if (inst->GetOpcode() == RISCVMIR::call)
    {
        return; // call指令可能涉及到跨函数调用，
    }
    //
    std::string opcode(magic_enum::enum_name(inst->GetOpcode()));
    RISCVBasicBlock *block = inst->GetParent();
    std::unique_ptr<RISCVFrame> &frame = block->GetParent()->GetFrame();
    // 创建操作数
    std::string name = tempfloat->Getname();
    VirRegister *lui_rd = new VirRegister(RISCVType::riscv_ptr);
    LARegister *lui_rs = new LARegister(RISCVType::riscv_ptr, name);
    VirRegister *flw_rd = new VirRegister(RISCVType::riscv_float32);
    LARegister *flw_rs = new LARegister(RISCVType::riscv_ptr, name, lui_rd);
    // 构造lui和flw指令
    RISCVMIR *lui = new RISCVMIR(RISCVMIR::RISCVMIR::RISCVISA::_lui);
    lui->SetDef(lui_rd);
    lui->AddOperand(lui_rs);
    frame->AddCantBeSpill(lui_rd);

    RISCVMIR *flw = new RISCVMIR(RISCVMIR::RISCVMIR::RISCVISA::_flw);
    flw->SetDef(flw_rd);
    flw->AddOperand(flw_rs);
    it.InsertBefore(lui);
    it.InsertBefore(flw);
    // 替换inst的立即数操作数，将used替换为flw_rd
    for (int i = 0; i < inst->GetOperandSize(); i++)
    {
        while (inst->GetOperand(i) == used)
        {
            inst->SetOperand(i, flw_rd);
        }
    }
}
// print
void dataSegment::PrintDataSegment_Globval()
{
    for (auto &gvar : globlvar_list)
    {
        gvar->PrintGloblvar();
    }
}
void dataSegment::PrintDataSegment_Tempvar()
{
    for (auto &gvar : tempvar_list)
    {
        gvar->PrintTempvar();
    }
}
// 将全局变量转换为 RISC-V 兼容的地址访问方式（lui + ld/sd 或 lui + addi）。
void dataSegment::LegalizeGloablVar(RISCVLoweringContext &ctx)
{
    std::map<globlvar *, VirRegister *> attached_normal; // 已处理的普通全局变量
    std::map<globlvar *, VirRegister *> attached_mem;    // 已处理的内存相关全局变量
    RISCVFunction *cur_func = ctx.GetCurFunction();      // 当前处理的RISCV函数
    for (auto block : *cur_func)
    {
        attached_normal.clear();
        attached_mem.clear();
        for (List<RISCVBasicBlock, RISCVMIR>::iterator it = block->begin(); it != block->end(); ++it)
        {
            auto inst = *it; // 获取当前指令mir
            for (int i = 0; i < inst->GetOperandSize(); i++)
            {
                if (globlvar *gvar = dynamic_cast<globlvar *>(inst->GetOperand(i)))
                {
                    std::unique_ptr<RISCVFrame> &frame = cur_func->GetFrame();
                    RISCVMIR::RISCVISA opcode = inst->GetOpcode();
                    // call指令，直接跳过不转换
                    if (opcode == RISCVMIR::RISCVISA::call)
                    {
                        continue;
                    }
                    // lui .1, %hi(name)
                    // ld/sd .2, %lo(name)(.1)
                    if ((opcode > RISCVMIR::RISCVISA::BeginMem && opcode < RISCVMIR::RISCVISA::EndMem) || (opcode > RISCVMIR::RISCVISA::BeginFloatMem && opcode < RISCVMIR::RISCVISA::EndFloatMem))
                    {
                        if (attached_mem.find(gvar) != attached_mem.end())
                        {
                            LARegister *lo_lareg = new LARegister(RISCVType::riscv_ptr, gvar->GetName(), dynamic_cast<VirRegister *>(attached_mem[gvar]));
                            inst->SetOperand(i, lo_lareg);
                        }
                        else
                        {
                            RISCVMIR *hi = new RISCVMIR(RISCVMIR::RISCVISA::_lui);
                            VirRegister *hi_vreg = ctx.createVReg(RISCVType::riscv_ptr);
                            frame->AddCantBeSpill(hi_vreg);
                            LARegister *hi_lareg = new LARegister(RISCVType::riscv_ptr, gvar->GetName());
                            hi->SetDef(hi_vreg);
                            hi->AddOperand(hi_lareg);
                            it.InsertBefore(hi);
                            LARegister *lo_lareg = new LARegister(RISCVType::riscv_ptr, gvar->GetName(), dynamic_cast<VirRegister *>(hi->GetDef()));
                            inst->SetOperand(i, lo_lareg);
                            attached_mem[gvar] = hi_vreg;
                        }
                    }
                    // lui .1, %hi(name)
                    // addi .2, %lo(name)
                    else
                    {
                        if (attached_normal.find(gvar) != attached_normal.end())
                        {
                            inst->SetOperand(i, attached_normal[gvar]);
                        }
                        else
                        {
                            RISCVMIR *hi = new RISCVMIR(RISCVMIR::RISCVISA::_lui);
                            VirRegister *hi_vreg = ctx.createVReg(RISCVType::riscv_ptr);
                            frame->AddCantBeSpill(hi_vreg);
                            LARegister *hi_lareg = new LARegister(RISCVType::riscv_ptr, gvar->GetName());
                            hi->SetDef(hi_vreg);
                            hi->AddOperand(hi_lareg);
                            it.InsertBefore(hi);

                            RISCVMIR *lo = new RISCVMIR(RISCVISA::_addi);
                            VirRegister *lo_vreg = ctx.createVReg(RISCVType::riscv_ptr);
                            frame->AddCantBeSpill(lo_vreg);
                            LARegister *lo_lareg = new LARegister(RISCVType::riscv_ptr, gvar->GetName(), LARegister::LAReg::lo);
                            lo->SetDef(lo_vreg);
                            lo->AddOperand(hi->GetDef());
                            lo->AddOperand(lo_lareg);
                            it.InsertBefore(lo);

                            inst->SetOperand(i, lo_vreg);
                            attached_normal[gvar] = lo_vreg;
                        }
                    }
                }
            }
        }
    }
}
// globlvar
globlvar::globlvar(Var *data) : RISCVGlobalObject(data->GetType(), data->GetName())
{
    IR_DataType tp = (dynamic_cast<PointerType *>(data->GetType()))->GetSubType()->GetTypeEnum();
    // 处理指针类型变量👆
    // 初始化👇（int float）
    if (tp == IR_DataType::IR_Value_INT || tp == IR_DataType::IR_Value_Float)
    {
        align = 2;
        size = 4;
        if (data->GetInitializer())
        {
            if (tp == IR_DataType::IR_Value_INT)
            {
                std::string num = data->GetInitializer()->GetName();
                int init = std::stoi(num);
                init_vector.push_back(init);
            }
            if (tp == IR_DataType::IR_Value_Float)
            {
                ConstIRFloat *temp = dynamic_cast<ConstIRFloat *>(data->GetInitializer());
                float init = temp->GetVal();
                init_vector.push_back(init);
            }
        }
    } //(数组)
    else if (tp == IR_DataType::IR_ARRAY)
    {
        align = 3;
        Type *basetype = dynamic_cast<HasSubType *>(data->GetType())->GetBaseType();
        if (Initializer *arry_init = dynamic_cast<Initializer *>(data->GetInitializer()))
        {
            size = arry_init->GetType()->GetSize();
            int init_size = arry_init->size();
            if (init_size == 0)
            {
                // sec = "bss";（未初始化）
            }
            else
            {
                // sec = "data";（已初始化）
                int limi = dynamic_cast<ArrayType *>(arry_init->GetType())->GetTypeEnum();
                for (int i = 0; i < limi; i++)
                {
                    if (i < init_size)
                    {
                        if (auto inits = dynamic_cast<Initializer *>((*arry_init)[i]))
                        {
                            // 递归处理子数组
                            generate_array_init(inits, basetype);
                        }
                        else
                        { // Leaf
                            if (basetype->GetTypeEnum() == IR_Value_INT)
                            {
                                std::string num = (*arry_init)[i]->GetName();
                                int init = std::stoi(num);
                                init_vector.push_back(init);
                            }
                            else if (basetype->GetTypeEnum() == IR_Value_Float)
                            {
                                ConstIRFloat *temp = dynamic_cast<ConstIRFloat *>((*arry_init)[i]);
                                float init = temp->GetVal();
                                init_vector.push_back(init);
                            }
                        }
                    }
                    else
                    { // 处理数组的默认初始化，补充0
                        Type *temptp = dynamic_cast<ArrayType *>(arry_init->GetType())->GetSubType();
                        size_t zeronum = temptp->GetSize() / basetype->GetSize();
                        for (int i = 0; i < zeronum; i++)
                        {
                            if (basetype->GetTypeEnum() == IR_Value_INT)
                            {
                                init_vector.push_back(static_cast<int>(0));
                            }
                            else if (basetype->GetTypeEnum() == IR_Value_Float)
                            {
                                init_vector.push_back(static_cast<float>(0));
                            }
                        }
                    }
                }
            }
        }
        else
        { // 处理无初始化的指针
            size = (dynamic_cast<PointerType *>(data->GetType()))->GetSubType()->GetSize();
        }
    }
    else
        align = -1; // Error
}
// 递归处理 Initializer* 结构，填充 init_vector。
void globlvar::generate_array_init(Initializer *arry_init, Type *basetype)
{
    int init_size = arry_init->size();
    int limi = dynamic_cast<ArrayType *>(arry_init->GetType())->GetNum();
    if (init_size == 0)
    {
        auto zero_num = arry_init->GetType()->GetSize() / basetype->GetSize();
        for (auto i = 0; i < zero_num; i++)
        {
            if (basetype->GetTypeEnum() == IR_Value_INT)
            {
                init_vector.push_back(static_cast<int>(0));
            }
            else
            {
                init_vector.push_back(static_cast<float>(0));
            }
        }
    }
    else
    {
        for (int i = 0; i < limi; i++)
        {
            if (i < init_size)
            {
                if (auto inits = dynamic_cast<Initializer *>((*arry_init)[i]))
                    generate_array_init(inits, basetype);
                else
                {
                    if (basetype->GetTypeEnum() == IR_Value_INT)
                    {
                        std::string num = (*arry_init)[i]->GetName();
                        int init = std::stoi(num);
                        init_vector.push_back(init);
                    }
                    else if (basetype->GetTypeEnum() == IR_Value_Float)
                    {
                        ConstIRFloat *temp = dynamic_cast<ConstIRFloat *>((*arry_init)[i]);
                        float init = temp->GetVal();
                        init_vector.push_back(init);
                    }
                }
            }
            else
            {
                Type *temptp = dynamic_cast<ArrayType *>(arry_init->GetType())->GetSubType();
                size_t zeronum = temptp->GetSize() / basetype->GetSize();
                for (int i = 0; i < zeronum; i++)
                {
                    if (basetype->GetTypeEnum() == IR_Value_INT)
                    {
                        init_vector.push_back(static_cast<int>(0));
                    }
                    else if (basetype->GetTypeEnum() == IR_Value_Float)
                    {
                        init_vector.push_back(static_cast<float>(0));
                    }
                }
            }
        }
    }
}
// can be:void globlvar::PrintGloblvar() {
//     std::string varName = this->GetName(); // 预先获取变量名，避免重复调用

//     // 处理未初始化的全局变量（放入 .bss 段）
//     if (init_vector.empty()) {
//         std::cout << "    .globl  " << varName << std::endl;
//         PrintSegmentType(BSS, oldtype);
//         std::cout << "    .align  " << align << std::endl;
//         std::cout << "    .type  " << varName << ", @" << ty << std::endl;
//         std::cout << "    .size  " << varName << ", " << size << std::endl;
//         std::cout << varName << ":" << std::endl;
//         std::cout << "    .zero  " << size << std::endl;
//         return;
//     }

//     // 处理已初始化的全局变量（放入 .data 段）
//     std::cout << "    .globl  " << varName << std::endl;
//     PrintSegmentType(DATA, oldtype);
//     std::cout << "    .align  " << align << std::endl;
//     std::cout << "    .type  " << varName << ", @" << ty << std::endl;
//     std::cout << "    .size  " << varName << ", " << size << std::endl;
//     std::cout << varName << ":" << std::endl;

//     int zero_count = 0;

//     for (size_t i = 0; i < init_vector.size(); i++) {
//         auto& init = init_vector[i];

//         // 判断是否为 0
//         bool is_zero = std::visit([](auto&& value) { return value == 0; }, init);

//         if (is_zero) {
//             zero_count += 4;
//         } else {
//             // 如果之前有连续 0，则输出 .zero
//             if (zero_count > 0) {
//                 std::cout << "    .zero  " << zero_count << std::endl;
//                 zero_count = 0;
//             }

//             // 处理整数类型
//             if (std::holds_alternative<int>(init)) {
//                 std::cout << "    .word  " << std::get<int>(init) << std::endl;
//             }
//             // 处理浮点数类型
//             else {
//                 uint32_t floatBits = *reinterpret_cast<uint32_t*>(&std::get<float>(init));
//                 std::cout << "    .word  " << floatBits << std::endl;
//             }
//         }
//     }

//     // 如果最后还有零，则补充 .zero 指令
//     if (zero_count > 0) {
//         std::cout << "    .zero  " << zero_count << std::endl;
//     }
// }

void globlvar::PrintGloblvar()
{
    if (init_vector.empty())
    {
        std::cout << "    .globl  " << this->GetName() << std::endl;
        PrintSegmentType(BSS, oldtype);
        std::cout << "    .align  " << align << std::endl;
        std::cout << "    .type  " << this->GetName() << ", @" << ty << std::endl;
        std::cout << "    .size  " << this->GetName() << ", " << size << std::endl;
        std::cout << this->GetName() << ":" << std::endl;
        std::cout << "    .zero  " << size << std::endl;
    }
    else
    {
        std::cout << "    .globl  " << this->GetName() << std::endl;
        PrintSegmentType(DATA, oldtype);
        std::cout << "    .align  " << align << std::endl;
        std::cout << "    .type  " << this->GetName() << ", @" << ty << std::endl;
        std::cout << "    .size  " << this->GetName() << ", " << size << std::endl;
        std::cout << this->GetName() << ":" << std::endl;
        int zero_count = 0;
        for (auto &init : init_vector)
        {
            bool is_zero = std::visit([](auto &&value)
                                      { return value == 0; }, init);
            if (is_zero)
            {
                zero_count += 4;
            }
            else
            {
                if (zero_count != 0)
                {
                    std::cout << "    .zero  " << zero_count << std::endl;
                    zero_count = 0;
                }

                if (std::holds_alternative<int>(init))
                {
                    std::cout << "    .word  " << std::get<int>(init) << std::endl;
                }
                else
                {
                    // float type
                    FloatBits bits;
                    bits.floatValue = std::get<float>(init);
                    std::string binaryString = std::bitset<32>(bits.intBits).to_string();
                    int decNum = binaryToDecimal(binaryString);
                    std::cout << "    .word  " << decNum << std::endl;
                }
            }
            if (&init == &init_vector.back() && is_zero)
            {
                std::cout << "    .zero  " << zero_count << std::endl;
            }
        }
    }
}
// tempvar
tempvar::tempvar(int num_lable, float init) : RISCVTempFloatObject("file"), num_lable(num_lable), align(2), init(init)
{
    this->GetName() = ".LC" + std::to_string(num_lable);
}
std::string tempvar::Getname()
{
    return this->GetName();
}
// can be:void tempvar::PrintTempvar() {
//     PrintSegmentType(RODATA, oldtype);
//     std::cout << "    .align  " << align << std::endl;
//     std::cout << this->GetName() << ":" << std::endl;
//     uint32_t floatBits = *reinterpret_cast<uint32_t*>(&init);
//     std::cout << "    .word  " << floatBits << std::endl;
// }

void tempvar::PrintTempvar()
{
    PrintSegmentType(RODATA, oldtype);
    std::cout << "    .align  " << align << std::endl;
    std::cout << this->GetName() << ":" << std::endl;
    FloatBits bits;
    bits.floatValue = init;
    std::string binaryString = std::bitset<32>(bits.intBits).to_string();
    int decNum = binaryToDecimal(binaryString);
    std::cout << "    .word  " << decNum << std::endl;
}
// textSegment
textSegment::textSegment(RISCVLoweringContext &ctx)
{
    GenerateFuncList(ctx);
}
void textSegment::GenerateFuncList(RISCVLoweringContext &ctx)
{
    for (auto &function : ctx.GetFunctions())
    {
        functionSegment *funcSeg = new functionSegment(function.get());
        function_list.push_back(funcSeg);
    }
}
void textSegment::PrintTextSegment()
{
    PrintSegmentType(TEXT, oldtype);
    for (auto &functionSegment : function_list)
    {
        functionSegment->PrintFuncSegment();
    }
}
// functionSegment
functionSegment::functionSegment(RISCVFunction *function) : func(function)
{
    align = 1;                  // bushi4?
    name = function->GetName(); // MIR
}
void functionSegment::PrintFuncSegment()
{
    std::cout << "    .align  " << align << std::endl;
    std::cout << "    .globl  " << name << std::endl;
    std::cout << "    .type  " << name << ", @" << ty << std::endl;
    func->printfull(); // MIR
    if (size == -1)
    {
        std::cout << "    .size" << name << ", " << ".-" << name << std::endl;
    }
}