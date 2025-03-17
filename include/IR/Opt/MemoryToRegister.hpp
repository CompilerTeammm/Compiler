#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"
#include "Passbase.hpp"
#include <map>
#pragma once

// 寻找定义和使用 alloc 的基本块
class AllocaInfo 
{
public:
    std::vector<BasicBlock*> DefBlocks;   // store 指令的BBs集合
    std::vector<BasicBlock*> UsingBlocks;   // load 指令的BBs集合
    
    BasicBlock * OnlyOneBk;   // 记录alloca 的def 和 users的唯一的一个基本块  对应仅在一个基本块中def 与 using 的基本块
    StoreInst* OnlyStoreInst;   // store 语句实际上是对alloca 的def  对应alloca仅有一个def的情况
    bool OnlyUsedInOneBlock;  // alloca 的读写（使用）操作判断是不是均在一个基本块中完成的
    //size_t  BasicBlocknums; // 记录一下storeinst 的数目 真鸡肋，，，

    //Value* AllocaPointerVal;  // 目前没有发现llvm为啥要加这个，先注释掉

    void AnalyzeAlloca(AllocaInst* AI);

    AllocaInfo()
      :OnlyOneBk(nullptr), OnlyStoreInst(nullptr),OnlyUsedInOneBlock(true)
    {  
      DefBlocks.clear(),UsingBlocks.clear();
    }

};

// Alloca分析之后存在该问题
// BlockInfo 用于记录和获取同一基本块中出现的 load 和 store 指令的先后顺序
class BlockInfo 
{
    // first  second 
    std::map<Instruction*,int> InstNumbersIndex;
public:
    BlockInfo() = default;
    int GetInstIndex(Instruction* Inst);

    void DeletIndex(Instruction* Inst) { InstNumbersIndex.erase(Inst); }
    bool isInterestingInstruction(List<BasicBlock, Instruction>::iterator Inst);

    void clear() { InstNumbersIndex.clear();}
};

class RenamePassData{
public:
    typedef std::vector<Value*> ValVector;

    RenamePassData()
    : BB(nullptr),PredBB(nullptr),Values()  {}

    RenamePassData(BasicBlock*B,BasicBlock*P , ValVector& V)
                :BB(B),PredBB(P),Values(V) {}

    BasicBlock* BB;
    BasicBlock* PredBB;
    ValVector Values;
};

class PromoteMem2Reg
{
public:
  // 遍历基本块中的指令，将指令进行一个消除 alloca/ store / load指令
    PromoteMem2Reg(Function* function, DominantTree* tree) 
                  :_func(function), _tree(tree) {}
    static bool isAllocaPromotable(AllocaInst* AI);

    bool promoteMemoryToRegister(DominantTree* tree,Function *func,std::vector<AllocaInst *>& Allocas);

    void RemoveFromAList(int& AllocaNum);

    void removeLifetimeIntrinsicUsers(AllocaInst* AI);

    bool rewriteSingleStoreAlloca(AllocaInfo& info,AllocaInst *AI,  BlockInfo& BBInfo);

    bool promoteSingleBlockAlloca(AllocaInfo& Info, AllocaInst* AI,BlockInfo& BkInfo);

    bool QueuePhiNode(BasicBlock* BB, int AllocaNum);

    void ComputeLiveInBlocks(AllocaInst* AI,std::set<BasicBlock*>& DefBlock,
                        std::set<BasicBlock*>& LiveInBlocks,AllocaInfo& info);

    //重命名的函数
    void RenamePass(BasicBlock* BB,BasicBlock* Pred,
                    RenamePassData::ValVector& IncomingVals,
                    std::vector<RenamePassData>& WorkList);


protected:
    Function *_func;
    DominantTree * _tree;
    std::vector<AllocaInst *> Allocas;
    std::map<AllocaInst* ,int> AllocaLookup;
    std::map<std::pair<int,int>,PhiInst*> NewPhiNodes;

    // BBs in Function number
    std::map<std::shared_ptr<BasicBlock>,int> BBNumbers;

    std::map<PhiInst*,int> PhiToAllocaMap;
    // 记录Phi和alloca的对应的关系
    // std::map<PhiInst*,int> PhiToAllocaMap;
};



// mem2reg pass 对应类是 PromoteLegacyPass  是一个FunctionPass  调用 函数
//  promoteMemoryToRegister  收集 Promotable 的 AllocaInst 然后调用函数 PromoteMemToReg
// AllocaInst 是 Promotable 的， AllocaInst 没有被用于 volatile instruction 并且它直接被用于 LoadInst 或 StoreInst（即没有被取过地址）
//   isAllocaPromotable   则认为该 AllocaInst 是 Promotable 
//  for 循环依次处理每一个 AllocaInst ，
