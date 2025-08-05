#include "../include/MyBackend/ProloAndEpilo.hpp"
#include "../include/MyBackend/MIR.hpp"
#include <memory>
#define INITSIZE 1

// 栈帧的开辟是定死的，我对此进行一个封装
// 函数如果调用其他函数才会存储 ra
// addi   sp,sp,-Malloc
// sd  ra,offset1
// sd  s0,offset2
// addi s0,sp,Malloc
// ....
// ld  ra,offset1
// ld s0,offset2
// addi   sp,sp,Malloc

// I think the std::move is not the must, Compiler will do this
// I hope to search it.

// array  stack 传参
bool ProloAndEpilo:: DealStoreInsts()
{
    auto MallocVec = mfunc->getStoreInsts();
    // auto StoreRecord = mfunc->getStoreRecord();
    auto& AOffsetRecord = mfunc->getAOffsetRecord();
    size_t offset = TheSizeofStack;
    std::set<AllocaInst*> tmp;
    for (auto& alloc:mfunc->getAllocas())
    {
        AOffsetRecord[alloc] = offset;
        auto it = alloc->GetType()->GetLayer();
        auto it2 =  dynamic_cast<PointerType*> (alloc->GetType())->GetSubType()->GetTypeEnum();
        if(alloc->GetType()->GetLayer() > 1)
            if (dynamic_cast<PointerType*>(alloc->GetType())->GetSubType()->GetTypeEnum() == IR_ARRAY)
                offset += 0;
            else 
                offset -= 8;
        else 
            offset -= 4;
    }

    for(auto[StackInst,alloc] : MallocVec)
    {
        if(AOffsetRecord[alloc] <= 2047)
            StackInst->setStoreStackOp(AOffsetRecord[alloc]);
        // if (tmp.find(alloc) == tmp.end())
        // {
        //     tmp.emplace(alloc);
        //     offset += 4;
        //     StackInst->setStoreStackOp(offset);
        //     AOffsetRecord.emplace(alloc,offset);
        // }
        // else {
        //     StackInst->setStoreStackOp(AOffsetRecord[alloc]);
        // }
    }

    return true;
}

bool ProloAndEpilo:: DealLoadInsts()
{
    auto LoadInsts = mfunc->getLoadInsts();
    auto record = mfunc->getLoadRecord();
    auto& offset = mfunc->getAOffsetRecord();
    for (auto Inst : LoadInsts)
    {
        auto Alloc = record[Inst];
        size_t off = offset[Alloc];
        if (off <= 2047)
            Inst->setStoreStackOp(off);  
        else  {
           
        }
    }

    // auto LoadInsts = mfunc->getLoadInsts();
    // auto record = mfunc->getLoadRecord();
    // auto storeRecord = mfunc->getStoreRecord();
    // for(auto Inst : LoadInsts)
    // {
    //     auto Alloc = record[Inst];
    //     auto Store = storeRecord[Alloc];
    //     if (Store != nullptr)
    //         Inst->getOpsVec().push_back(Store->getOpreand(1));
    // }

    return true;
}

bool ProloAndEpilo::run()
{
    size_t StackMallocSize = caculate();

    TheSizeofStack = StackMallocSize;

    if(TheSizeofStack <= 2047) { 
    // 生成函数的前言和后序,并且set了
        CreateProlo(StackMallocSize);
        CreateEpilo(StackMallocSize);
    } else {
        DealExtraProlo(StackMallocSize);
        DealExtraEpilo(StackMallocSize);
    }

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
        if(allocInst->GetTypeEnum() == IR_PTR) 
        {
            auto PType = dynamic_cast<PointerType*> (allocInst->GetType());
            if (PType->GetSubType()->GetTypeEnum() == IR_ARRAY)
            {

            }
            else if (PType->GetSubType()->GetTypeEnum() == IR_PTR)
            {
                size_t ptrSize = PType->GetSubType()->GetSize();
                sumMallocSize += ptrSize;
            }
            else if(PType->GetSubType()->GetTypeEnum() == IR_Value_Float ||
                  PType->GetSubType()->GetTypeEnum() == IR_Value_INT ) {
                sumMallocSize += sizeof(int32_t);
            }
        }
    }
    if (mfunc->arroffset != mfunc->defaultSize)  // 未开辟数组
        sumMallocSize += mfunc->arroffset;
    while (N * ALIGN < sumMallocSize)   N++;

    size_t size = (N + INITSIZE) * ALIGN;
    return size; 
}


void ProloAndEpilo::SetSPOp(std::shared_ptr<RISCVInst> inst,size_t size,bool flag)
{
    inst->SetRegisterOp(std::move("sp"),Register::real);
    inst->SetRegisterOp (std::move("sp"),Register::real);

    if(flag == _malloc)
        inst->SetstackOffsetOp(std::move("-"+std::to_string(size)));
    else 
        inst->SetstackOffsetOp(std::move(std::to_string(size)));
}
void ProloAndEpilo::SetsdRaOp(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("ra",Register::real);
    inst->SetRegisterOp(std::to_string(size-8)+"(sp)",Register::real);
}
void ProloAndEpilo::SetsdS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("s0",Register::real);
    inst->SetRegisterOp(std::to_string(size-16)+"(sp)",Register::real);
}
void ProloAndEpilo::SetS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("s0",Register::real);
    inst->SetRegisterOp("sp",Register::real);
    
    inst->SetstackOffsetOp(std::to_string(size));

}

void ProloAndEpilo::SetldRaOp(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("ra",Register::real);
    inst->SetRegisterOp (std::to_string(size-8)+"(sp)",Register::real);
}
void ProloAndEpilo::SetldS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRegisterOp("s0",Register::real);
    inst->SetRegisterOp (std::to_string(size-16)+"(sp)",Register::real);
}

void ProloAndEpilo::CreateProlo(size_t size)
{
    auto it = std::make_shared<RISCVPrologue> ();
    auto& InstVec = it->getInstsVec();
    
    // 开辟栈帧
    auto spinst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetSPOp(spinst,size);
    InstVec.push_back(spinst);

    auto sdRaInst = std::make_shared<RISCVInst> (RISCVInst::_sd);
    SetsdRaOp(sdRaInst, size);
    InstVec.push_back(sdRaInst);

    // 这个存储我认为可能是需要for函数去遍历这个
    auto sdS0inst = std::make_shared<RISCVInst> (RISCVInst::_sd); 
    SetsdS0Op(sdS0inst,size);
    InstVec.push_back(sdS0inst);

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

    auto ldRaInst = std::make_shared<RISCVInst> (RISCVInst::_ld);
    SetldRaOp(ldRaInst,size); 
    InstVec.push_back(ldRaInst);

    auto ldS0inst = std::make_shared<RISCVInst> (RISCVInst::_ld);
    SetldS0Op(ldS0inst,size); 
    InstVec.push_back(ldS0inst);

    auto spinst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetSPOp(spinst,size,_free);
    InstVec.push_back(spinst);

    mfunc->setEpilogue(it);
}

void ProloAndEpilo::DealExtraProlo(size_t size)
{
    size_t defaultSize = 16;
    auto it = std::make_shared<RISCVPrologue> ();
    auto& InstVec = it->getInstsVec();
    
    // 开辟栈帧
    auto spinst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetSPOp(spinst,defaultSize);
    InstVec.push_back(spinst);

    auto sdRaInst = std::make_shared<RISCVInst> (RISCVInst::_sd);
    SetsdRaOp(sdRaInst, defaultSize);
    InstVec.push_back(sdRaInst);

    // 这个存储我认为可能是需要for函数去遍历这个
    auto sdS0inst = std::make_shared<RISCVInst> (RISCVInst::_sd); 
    SetsdS0Op(sdS0inst,defaultSize);
    InstVec.push_back(sdS0inst);

    // 栈指针的赋值
    auto s0inst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetS0Op(s0inst,defaultSize);
    InstVec.push_back(s0inst);

    auto liInst = std::make_shared<RISCVInst> (RISCVInst::_li);
    liInst->SetRealRegister("t0");
    liInst->SetImmOp(ConstIRInt::GetNewConstant(defaultSize-size));
    InstVec.push_back(liInst);

    auto addInst = std::make_shared<RISCVInst>(RISCVInst::_add);
    addInst->SetRealRegister("sp");
    addInst->SetRealRegister("sp");
    addInst->SetRealRegister("t0");
    InstVec.push_back(addInst);


    mfunc->setPrologue(it);
}

void ProloAndEpilo::DealExtraEpilo(size_t size)
{
    size_t defaultSize = 16;
    auto it = std::make_shared<RISCVEpilogue> ();
    auto& InstVec = it->getInstsVec();

    auto liInst = std::make_shared<RISCVInst> (RISCVInst::_li);
    liInst->SetRealRegister("t0");
    liInst->SetImmOp(ConstIRInt::GetNewConstant(size-defaultSize));
    InstVec.push_back(liInst);

    auto addInst = std::make_shared<RISCVInst>(RISCVInst::_add);
    addInst->SetRealRegister("sp");
    addInst->SetRealRegister("sp");
    addInst->SetRealRegister("t0");
    InstVec.push_back(addInst);

    auto ldRaInst = std::make_shared<RISCVInst> (RISCVInst::_ld);
    SetldRaOp(ldRaInst,defaultSize); 
    InstVec.push_back(ldRaInst);

    auto ldS0inst = std::make_shared<RISCVInst> (RISCVInst::_ld);
    SetldS0Op(ldS0inst,defaultSize); 
    InstVec.push_back(ldS0inst);

    auto spinst = std::make_shared<RISCVInst> (RISCVInst::_addi);
    SetSPOp(spinst,defaultSize,_free);
    InstVec.push_back(spinst);

    mfunc->setEpilogue(it);
}