#include "../include/MyBackend/RISCVPrint.hpp"
#include "../include/lib/Type.hpp"

void TextSegment::generate_array_init(Initializer* arry_init, Type* basetype) {
    if(basetype->GetTypeEnum() == IR_Value_INT) {
        setArrIntOrFloat() = 0 ;
    } else {
        setArrIntOrFloat() = 1;
    }
    int init_size = arry_init->size();
    int limi = dynamic_cast<ArrayType*>(arry_init->GetType())->GetNum();
    if (init_size == 0) {
        auto zero_num=arry_init->GetType()->GetSize()/basetype->GetSize();
        for(auto i=0;i<zero_num;i++){
            if(basetype->GetTypeEnum()==IR_Value_INT)init_vector.push_back(static_cast<int>(0));
            else init_vector.push_back(static_cast<float>(0));
        }
    }
    else {
        for(int i=0; i<limi; i++) {
            if(i<init_size) {
                if(auto inits=dynamic_cast<Initializer*>((*arry_init)[i]))
                    generate_array_init(inits, basetype);
                else {
                        if(basetype->GetTypeEnum() == IR_Value_INT) {
                            std::string num = (*arry_init)[i]->GetName();
                            int init = std::stoi(num);
                            init_vector.push_back(init);
                        }
                        else if (basetype->GetTypeEnum() == IR_Value_Float) {
                            ConstIRFloat* temp = dynamic_cast<ConstIRFloat*>((*arry_init)[i]);
                            float init = temp->GetVal();
                            init_vector.push_back(init);
                        }                    
                }
            }
            else {
                Type* temptp = dynamic_cast<ArrayType*>(arry_init->GetType())->GetSubType();
                size_t zeronum = temptp->GetSize() / basetype->GetSize();
                    for(int i=0; i<zeronum; i++) {
                    if (basetype->GetTypeEnum() == IR_Value_INT) {
                        init_vector.push_back(static_cast<int>(0));
                    }
                    else if (basetype->GetTypeEnum() == IR_Value_Float) {
                        init_vector.push_back(static_cast<float>(0));   
                    }
                }
            }
        }
    }
}

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
            word = (Vusee->GetName());
        }
        else if(subType->GetTypeEnum() == IR_Value_Float)
        {
            uint32_t n;
            auto val = Vusee->GetName();
            float fval = std::stof(val);
            memcpy(&n, &fval, sizeof(float)); // 直接复制内存位模式
            word = (std::to_string(n));
        }
        else if (subType->GetTypeEnum() == IR_ARRAY)
        {
            ArrayType* arrType = dynamic_cast<ArrayType*> (Vusee->GetType());
            auto InitList = Vusee->as<Initializer>();
            generate_array_init(InitList,arrType->GetBaseType());
        }
    }
    else if(type == bss)
    {
        word = (std::to_string(defaultSize));
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
    Value* init = var->GetInitializer();
    if(init != nullptr)  {
        type = data;
        if(init->GetTypeEnum() == IR_ARRAY && 
           dynamic_cast<Initializer*>(var->GetInitializer())->size() == 0) 
        {
            type = bss;
        }
    }
    else {
        type = bss;
    }

    // if(var->GetInitializer() != nullptr      // Initializer 继承自 vector 
    //   && dynamic_cast<Initializer*>(var->GetInitializer())->size()!=0) {   // type
    //     type = data;
    // } else {
    //     type = bss;
    // }
    name = var->GetName();               // name
    if ( var->GetTypeEnum() == IR_PTR)
    {
        auto PType = dynamic_cast<PointerType*> (var->GetType());
        auto SubType = PType->GetSubType();   // sub 是上一级的
        // auto baseType = PType->GetBaseType();  base 是最基础的
        size = SubType->GetSize();     // size
        auto dataType = SubType->GetTypeEnum();

        if (dataType == IR_ARRAY) {    // align
            getArrFlag() = 1;
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

template<typename T>
void TextSegment::arrPrint(int& zeroFlag, int& zeroSizesum)
{
    for (auto &v : init_vector)
    {
        int i = std::get<T>(v);
        if (i == 0) {
            zeroFlag = 1;   zeroSizesum += 4;
        }
        else{
            if (zeroFlag == 1) {
                std::cout << "    " << ".zero" << "  " << std::to_string(zeroSizesum) << std::endl;
                zeroSizesum = 0;  zeroFlag = 0;
            }
            std::cout << "    " << ".word" << "  " << std::to_string(i) << std::endl;
        }
    }
}

void TextSegment::TextPrint()
{
    std::cout << "    " <<".globl"<<"	"<< name << std::endl; 
    std::cout << "    " << translateType() <<std::endl; 
    std::cout << "    " <<".align"<<"	"<< std::to_string(align) << std::endl; 
    std::cout << "    " <<".type"<<"  "<< name <<", "<<"@object"<< std::endl; 
    std::cout << "    " <<".size"<<"  "<< name <<", " <<size<< std::endl; 
    std::cout <<name <<":"<< std::endl;
    // if( type == 0) {
    //     std::cout << "    " <<".zero"<<"  "<< word[0] << std::endl;
    // } else {
    //     for(auto& w : word) 
    //         std::cout << "    " <<".word"<<"  "<< w << std::endl;
    // }
    if (type == bss)
        std::cout << "    " <<".zero"<<"  "<< word << std::endl;
    else if (type == data) 
    {
        if(getArrFlag() == 1) 
        {
            int zeroFlag = 0;
            int sum = 0;
            auto v = init_vector[0];
            if (std::holds_alternative<int>(init_vector[0]))
            {
                arrPrint<int>(zeroFlag,sum);
            }
            else if (std::holds_alternative<float>(v))
            {
                arrPrint<float>(zeroFlag,sum);
            }
            if (zeroFlag == 1)
                    std::cout << "    " <<".zero"<<"  "<< std::to_string(sum) << std::endl;
        } else {
            std::cout << "    " <<".word"<<"  "<< word << std::endl;
        }
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
    //  global values
    if(_context->getTexts().size() != 0)
        std::cout << "    " <<".text" << std::endl;
    for(auto text :_context->getTexts()) 
    {
        text->TextPrint();
    }
    // funcs
    auto funcs = _context->getMfuncs();
    std::cout << "    " <<".text" << std::endl;
    for(auto func : funcs)
    {
        // RISCVFunction
        std::cout << "    .align 1"<< std::endl;
        std::cout << "    .globl " << func->getName() << std::endl;
        std::cout << "    .type  "<< func->getName() << ", @function" << std::endl;
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
