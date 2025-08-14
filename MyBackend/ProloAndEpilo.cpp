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

//
// frame
// ra,s0
// local arrs
// local int/float/ptr
// spilled register
// caller saved register
// callee saved register
// spill arguments
//

// deal the fame and caculate the size  
size_t ProloAndEpilo::caculate()
{
    int N = INITSIZE;
    int sumMallocSize = 0;

    // deal local arr
    if (mfunc->arroffset != mfunc->defaultSize)
        sumMallocSize += mfunc->arroffset - mfunc->defaultSize;

    // local int/float/ptr
    for (auto allocInst : mfunc->getAllocas())
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
                if (sumMallocSize % 8 != 0)
                    sumMallocSize += 4;
            }
            else if(PType->GetSubType()->GetTypeEnum() == IR_Value_Float ||
                  PType->GetSubType()->GetTypeEnum() == IR_Value_INT ) {
                sumMallocSize += sizeof(int32_t);
            }
        }
    }

    // deal spilled register

    // caller saved register

    // callee saved register

    // deal spill arguments  small -> big
    
    // callee
    auto& LoadInstVec = mfunc->getSpilledParamLoadInst();
    size_t spillOffset = 0;
    for (auto& inst : LoadInstVec)
    {
        inst->SetstackOffsetOp(std::to_string(spillOffset) + "(s0)");
        spillOffset += 8;
    }

    // caller 
    auto& paraNum = mfunc->getNeadStackForparam();
    if ( paraNum  > 8) {
        sumMallocSize += (paraNum -8) * 8;
    }


    // result
    while (N * ALIGN < sumMallocSize)  N++;
    size_t size = (N +1)* ALIGN;  // 1 is for sp/ra
    return size; 
}

// array  stack 传参
bool ProloAndEpilo:: DealStoreInsts()
{
    auto MallocVec = mfunc->getStoreRecord();
    auto& AOffsetRecord = mfunc->getAOffsetRecord();
    size_t offset = mfunc->arroffset;
    std::set<AllocaInst*> tmp;
    for (auto& alloc:mfunc->getAllocas())
    {
        if(alloc->GetType()->GetLayer() > 1)
            if (dynamic_cast<PointerType*>(alloc->GetType())->GetSubType()->GetTypeEnum() == IR_ARRAY)
                offset -= 0;
            else  {  // ptr 8 字节对齐
                offset += 8;
                if (offset % 8 != 0)
                    offset += 4;
            }
        else 
            offset += 4;
        AOffsetRecord[alloc] = offset;
    }

    for(auto[StackInst,alloc] : MallocVec)
    {
        if(AOffsetRecord[alloc] <= 2047)
            StackInst->setStoreStackS0Op(AOffsetRecord[alloc]);
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
            Inst->setStoreStackS0Op(off);  
        else  {
           
        }
    }


    return true;
}

bool ProloAndEpilo::run()
{
    size_t StackMallocSize = caculate();

    TheSizeofStack = StackMallocSize;

    // 前言与后续的处理需要考虑callee_saved and  caller_saved
    if(TheSizeofStack <= 2047) { 
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

// TODO: reWrite the CreateEpilo And CreateEpilo
void ProloAndEpilo::SetSPOp(std::shared_ptr<RISCVInst> inst,size_t size,bool flag)
{
    inst->SetRealRegister(std::move("sp"));
    inst->SetRealRegister(std::move("sp"));

    if(flag == _malloc)
        inst->SetstackOffsetOp(std::move("-"+std::to_string(size)));
    else 
        inst->SetstackOffsetOp(std::move(std::to_string(size)));
}
void ProloAndEpilo::SetsdRaOp(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRealRegister("ra");
    inst->SetRealRegister(std::to_string(size-8)+"(sp)");
}
void ProloAndEpilo::SetsdS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRealRegister("s0");
    inst->SetRealRegister(std::to_string(size-16)+"(sp)");
}
void ProloAndEpilo::SetS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRealRegister("s0");
    inst->SetRealRegister("sp");
    
    inst->SetstackOffsetOp(std::to_string(size));

}

void ProloAndEpilo::SetldRaOp(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRealRegister("ra");
    inst->SetRealRegister(std::to_string(size-8)+"(sp)");
}
void ProloAndEpilo::SetldS0Op(std::shared_ptr<RISCVInst> inst,size_t size)
{
    inst->SetRealRegister("s0");
    inst->SetRealRegister(std::to_string(size-16)+"(sp)");
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