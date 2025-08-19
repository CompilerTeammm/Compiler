#pragma once
#include "../lib/CFG.hpp"
#include "../lib/CoreClass.hpp"
#include "MIR.hpp"
// as for a func

// saved register 保存
// 栈帧的布局
// 栈帧建立与销毁

// addi   sp,sp,-16
// sd  s0,8(sp)
// addi s0,sp,16
// ....
// ld s0,8(sp)
// addi   sp,sp,16

#define ALIGN 16
class ProloAndEpilo
{
    RISCVFunction* mfunc;
    size_t TheSizeofStack;
    size_t csrStart = 0;
public:
    enum mallOrfree
    {
        _malloc,  // 0
        _free     // 1
    };
    ProloAndEpilo(RISCVFunction* _mfunc) :mfunc(_mfunc)  { }
    bool run();

    void CreateProlo(size_t size);
    void CreateEpilo(size_t size);

    // 在这里设置他们的操作符, 开辟栈帧用的
    void SetSPOp(std::shared_ptr<RISCVInst> inst,size_t size,bool flag = _malloc);
    void SetsdRaOp(std::shared_ptr<RISCVInst> inst,size_t size);
    void SetsdS0Op(std::shared_ptr<RISCVInst> inst,size_t size);
    void SetS0Op(std::shared_ptr<RISCVInst> inst,size_t size);
    void SetldRaOp(std::shared_ptr<RISCVInst> inst,size_t size);
    void SetldS0Op(std::shared_ptr<RISCVInst> inst,size_t size);


    // 计算 mfunc 里面需要开辟多大的栈空间，来存储临时变量
    size_t caculate();
    bool DealStoreInsts();
    bool DealLoadInsts();
    
    // size > 2047 的情况
    void DealExtraEpilo(size_t size);
    void DealExtraProlo(size_t size);
};