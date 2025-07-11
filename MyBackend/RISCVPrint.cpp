#include "../include/MyBackend/RISCVPrint.hpp"
#include "../include/lib/Type.hpp"

void TextSegment::FillTheWord(size_t defaultSize)
{   
    // I already know the type of data ---》  .bss   .data
    // defaultSize   ==    this value's  size
    // this value  is an  Instruction
    // type value size -----> to fill the word

    auto var = dynamic_cast<Var*> (value);
    auto Vartype = var->GetType();
    auto PType = dynamic_cast<PointerType*> (Vartype);
    auto it = PType->GetBaseType();
    auto subType = PType->GetSubType();
    Value* Vusee = var->GetInitializer();

    if (type == data)
    {
        if (subType->GetTypeEnum() == IR_Value_INT)
        {
            word.emplace_back(Vusee->GetName());
        }
        else if(subType->GetTypeEnum() == IR_Value_Float)
        {
            uint32_t n;
            auto val = Vusee->GetName();
            float fval = std::stof(val);
            memcpy(&n, &fval, sizeof(float)); // 直接复制内存位模式
            word.emplace_back(std::to_string(n));
        }
        else if (subType->GetTypeEnum() == IR_ARRAY)
        {
            ArrayType* arrType = dynamic_cast<ArrayType*> (Vusee->GetType());
            auto InitList = Vusee->as<Initializer>();
            std::vector<int> index;
            auto val = InitList->GetInitVal(index);
            int layer = arrType->GetLayer();
            int a = 10;
        }
    }
    else if(type == bss)
    {
        word.emplace_back(std::to_string(defaultSize));
    }

    // auto var = dynamic_cast<Var*>(value);
    // if (type == data)
    // {
    //     auto init = var->GetInitializer();
    //     if (init->GetTypeEnum() == IR_Value_INT)
    //     {
    //         word.emplace_back(init->GetName());
    //     }
    //     else if (init->GetTypeEnum() == IR_Value_Float)
    //     {
    //         uint32_t n;
    //         auto val = init->GetName();
    //         float fval = std::stof(val);
    //         memcpy(&n, &fval, sizeof(float)); // 直接复制内存位模式
    //         word.emplace_back(std::to_string(n));
    //     }
    //     else if (init->GetTypeEnum() == IR_ARRAY) // ARRAY && init
    //     {
    //         auto arr = dynamic_cast<ArrayType *>(init->GetType());
    //         int layerSize = arr->GetLayer();
    //         int num = arr->GetNum();
    //         auto initList = init->as<Initializer>();
    //         for (int i = 1; i < layerSize; i++)
    //         {
    //         }
    //         for (auto &e : *initList)
    //         {
    //             auto interArr = dynamic_cast<ArrayType *>(e->GetType());
    //             auto interList = e->as<Initializer>();
    //             for (auto &interE : *interList)
    //             {
    //                 auto name = interE->GetName();
    //                 word.emplace_back(name);
    //             }
    //         }
    //     }
    // }
    // else
    // { // .bss
    //     word.emplace_back(std::to_string(defaultSize));
    // }
}

// need to deal the var
void TextSegment::TextInit()
{
    auto var = dynamic_cast<Var*> (value);
    if(var->GetInitializer() != nullptr) {   // type
        type = data;
    } else {
        type = bss;
    }
    name = var->GetName();               // name
    if ( var->GetTypeEnum() == IR_PTR)
    {
        auto PType = dynamic_cast<PointerType*> (var->GetType());
        auto SubType = PType->GetSubType();   // sub 是上一级的
        // auto baseType = PType->GetBaseType();  base 是最基础的
        size = SubType->GetSize();     // size
        auto dataType = SubType->GetTypeEnum();

        if (dataType == IR_ARRAY) {    // align
            align = 3;
        } else {
            align = 2;
        }
        
        FillTheWord(SubType->GetSize());
    }
    else {
        LOG(ERROR,"must be IR_PTR");
    }
}

std::string TextSegment::translateType()
{
    if( !type ) {
        return std::string(".bss");
    } else {
        return std::string(".data");
    }
}

void TextSegment::TextPrint()
{
    std::cout << "    " <<".global"<<"	"<< name << std::endl; 
    std::cout << "    " << translateType() <<std::endl; 
    std::cout << "    " <<".align"<<"	"<< std::to_string(align) << std::endl; 
    std::cout << "    " <<".type"<<"  "<< name <<", "<<"@object"<< std::endl; 
    std::cout << "    " <<".size"<<"  "<< name <<", " <<size<< std::endl; 
    std::cout <<name <<":"<< std::endl;
    if( type == 0) {
        std::cout << "    " <<".zero"<<"  "<< word[0] << std::endl;
    } else {
        for(auto& w : word) 
            std::cout << "    " <<".word"<<"  "<< w << std::endl;
    }
}   

void RISCVPrint::printPrefix()
{
    std::cout << "    " <<".file" << "    " <<"\"" << _fileName <<"\""<< std::endl;
    std::cout << "    " <<".option nopic" << std::endl;
    std::cout << "    " <<".attribute arch, \
    \"rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0\"" << std::endl;
    std::cout << "    " <<".attribute unaligned_access, 0" << std::endl;
    std::cout << "    " <<".attribute stack_align, 16" << std::endl;
    std::cout << "    " <<".text" << std::endl;
    std::cout << "    " <<".section	.text.startup,\"ax\",@progbits" << std::endl;

}

//	.size	main, .-main
void RISCVPrint::printFuncfix(std::string name)
{
    std::cout << "    " <<".size";
    std::cout << "	" << name <<",";
    std::cout <<" .-" << name << std::endl;
} 

void RISCVPrint::printInsts(RISCVInst* inst)
{
    std::cout << "    " << inst->ISAtoAsm() << "  ";
    int count = inst->getOpsVec().size() -1 ;
    for (auto &op : inst->getOpsVec())
    {
        std::cout << op->getName();

        if(count != 0){
            std::cout <<"," ;
            count--;
        }
    }
    std::cout << std::endl;
}

void RISCVPrint::printFuncPro(RISCVFunction* mfunc)
{
    auto it = mfunc->getPrologue();
    for(auto inst : it->getInstsVec())
    {
        printInsts(inst.get());
    }
}

void RISCVPrint::printFuncEpi(RISCVFunction* mfunc)
{
    auto it = mfunc->getEpilogue();
    for(auto inst : it->getInstsVec())
    {
        printInsts(inst.get());
    }
}

void RISCVPrint::printAsm()
{
    printPrefix();   
    for(auto text :_context->getTexts()) 
    {
        text->TextPrint();
    }
    auto funcs = _context->getMfuncs();
    for(auto func : funcs)
    {
        // RISCVFunction
        std::cout  << func->getName() << ": " <<std::endl;
        printFuncPro(func.get());

        for(auto bb : *func.get())
        {
            std::cout << bb -> getName() <<": " << std::endl;
            // 这个仅仅只要被执行一次即可
            for(auto inst : *bb)
            {
                if(inst->getOpcode() == RISCVInst::_ret)
                    printFuncEpi(func.get());
                printInsts(inst);
            }
        } 
        // auto test = func->getName();
        printFuncfix(func->getName());
    }
}
