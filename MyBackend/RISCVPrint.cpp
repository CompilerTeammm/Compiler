#include "../include/MyBackend/RISCVPrint.hpp"

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
