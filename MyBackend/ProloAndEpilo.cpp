#include "../include/MyBackend/ProloAndEpilo.hpp"
#define INITSIZE 1

// 栈帧的开辟是定死的，我对此进行一个封装
// addi   sp,sp,-Malloc
// sd  s0,offset
// addi s0,sp,Malloc
// ....
// ld s0,offset
// addi   sp,sp,Malloc

// I think the std::move is not the must, Compiler will do this
// I hope th search it.

bool ProloAndEpilo:: DealStoreInsts()
{
    auto MallocVec = mfunc->getStoreInsts();
    auto StoreRecord = mfunc->getStoreRecord();
    size_t offset = 16;
    std::set<AllocaInst*> tmp;
    for(auto[StackInst,alloc] : MallocVec)
    {
        if (tmp.find(alloc) == tmp.end())
        {
            tmp.emplace(alloc);
            offset += 4;
        }
        StackInst->setStoreStackOp(offset);
    }

    return true;
}

bool ProloAndEpilo:: DealLoadInsts()
{
    auto LoadInsts = mfunc->getLoadInsts();
    auto record = mfunc->getLoadRecord();
    auto storeRecord = mfunc->getStoreRecord();
    for(auto Inst : LoadInsts)
    {
        auto Alloc = record[Inst];
        auto Store = storeRecord[Alloc];
        Inst->getOpsVec().push_back(Store->getOpreand(1));
    }

    return true;
}

bool ProloAndEpilo::run()
{
    size_t StackMallocSize = caculate();

    // 生成函数的前言和后序,并且set了
    CreateProlo(StackMallocSize);
    CreateEpilo(StackMallocSize);

    bool ret = DealStoreInsts();
    ret = DealLoadInsts();
    return ret;
}

size_t ProloAndEpilo::caculate()
{
    int N = INITSIZE;
    int sumMallocSize = 0;

    for(auto allocInst : mfunc->getAllocas())
    {
        auto e = allocInst;
        if(e->GetTypeEnum() == IR_PTR) 
        {
            auto PType = dynamic_cast<PointerType*> (e->GetType());
            if (PType->GetSubType()->GetTypeEnum() == IR_ARRAY)
            {
                size_t arrSize = PType->GetSubType()->GetSize();
                sumMallocSize += arrSize;
            }
            else if(PType->GetSubType()->GetTypeEnum() == IR_Value_Float ||
                  PType->GetSubType()->GetTypeEnum() == IR_Value_INT ) {
                sumMallocSize += sizeof(int32_t);
            }
        }
    }
    while (N * ALIGN < sumMallocSize)   N++;

    size_t size = (N + INITSIZE) * ALIGN;
    return size; 
}

void ProloAndEpilo::SetSPOp(std::shared_ptr<RISCVInst> inst,size_t size,bool flag)
{
    inst->SetRegisterOp(std::move("sp"),Register::real);
    inst->SetRegisterOp (std::move("sp"),Register::real);

    if(flag == _malloc)
        inst->SetImmOp(std::move("-"+std::to_string(size)));
    else 
        inst->SetImmOp(std::move(std::to_string(size)));
}

void ProloAndEpilo::SetSDOp(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("s0",Register::real);
    inst->SetRegisterOp(std::to_string(size-8)+"(sp)",Register::real);
}

void ProloAndEpilo::SetS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("sp",Register::real);
    inst->SetRegisterOp("sp",Register::real);
    
    inst->SetImmOp(std::to_string(size));

}

void ProloAndEpilo::SetLDOp(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("s0",Register::real);
    inst->SetRegisterOp (std::to_string(size-8)+"(sp)",Register::real);
}

void ProloAndEpilo::CreateProlo(size_t size)
{
    auto it = std::make_shared<RISCVPrologue> ();
    auto& InstVec = it->getInstsVec();
    
    // 开辟栈帧
    auto spinst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetSPOp(spinst,size);
    InstVec.push_back(spinst);

    // 这个存储我认为可能是需要for函数去遍历这个
    auto sdinst = std::make_shared<RISCVInst> (RISCVInst::_sd); 
    SetSDOp(sdinst,size);
    InstVec.push_back(sdinst);

    // 栈指针的赋值
    auto s0inst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetS0Op(s0inst,size);
    InstVec.push_back(s0inst);

    mfunc->setPrologue(it);
}

void ProloAndEpilo::CreateEpilo(size_t size)
{
    auto it = std::make_shared<RISCVEpilogue> ();
    auto& InstVec = it->getInstsVec();

    auto ldinst = std::make_shared<RISCVInst> (RISCVInst::_ld);
    SetLDOp(ldinst,size); 
    InstVec.push_back(ldinst);

    auto spinst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetSPOp(spinst,size,_free);
    InstVec.push_back(spinst);

    mfunc->setEpilogue(it);
}