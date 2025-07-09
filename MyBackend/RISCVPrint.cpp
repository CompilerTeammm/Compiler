#include "../include/MyBackend/RISCVPrint.hpp"
#include "../include/lib/Type.hpp"

// 1.init 全局变量
// 2.处理数组的存在
// 3.函数调用的处理
// 4.函数参数的注意
// 5.代码的测评（抽样）

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
        auto SubType = PType->GetSubType();
        size = SubType->GetSize();     // size
        auto dataType = SubType->GetTypeEnum();

        if (dataType == IR_ARRAY) {    // align
            align = 3;
        } else {
            align = 2;
        }

        if (type == data) {
            auto init = var->GetInitializer();
            if ( init->GetTypeEnum() == IR_Value_INT){
                word = init->GetName();
            }
            else if (init->GetTypeEnum() == IR_Value_Float)
            {
                uint32_t n;
                auto val = init->GetName();
                float fval = std::stof(val);
                memcpy(&n, &fval, sizeof(float)); // 直接复制内存位模式
                word = std::to_string(n);
            }
        }
        else {
            word = std::to_string(SubType->GetSize());
        }
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
    std::cout <<name << std::endl;
    if( type == 0) {
        std::cout << "    " <<".zero"<<"  "<< word << std::endl;
    } else {
        std::cout << "    " <<".word"<<"  "<< word << std::endl;
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
