#pragma once
#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "../../include/IR/Opt/Men2reg.hpp"

// %op0 = alloca i32       开辟空间
// %op1 = add i32 1,2
// store i32 %op1, i32* %op0   定义，defB 
// %op2 = load i32, i32* %op0     使用 usingB
void AllocaInfo::AnalyzeAlloca(AllocaInst* AI)
{
    // 确定 该条 alloca 的使用情况 value->Use ->User
    for(Use*use : AI->GetValUseList())
    {
        User* user = use->GetUser();
        BasicBlock* tmpBB;
        // 到这里的指令仅仅是store 和 load 这两种，其他的不可能
        if(StoreInst* SInst = dynamic_cast<StoreInst*>(user))
        {
            // 储存bbs instrution -> bbs
            // BasicBlock* parent = nullptr; //临时测试用的
            tmpBB = SInst->GetParent();
            AllocaPointerVal = SInst->GetOperand(0);
            DefBlocks.push_back(tmpBB);
            // BasicBlocknums++;  说实话，这个鸡肋了，因为 defBlocks就可以求出里面的个数
            OnlyStoreInst =SInst;
        }
        else{
            LoadInst* LInst = dynamic_cast<LoadInst*>(user);
            tmpBB = LInst->GetParent();
            // BasicBlock* parent = nullptr; //临时测试用的
            UsingBlocks.push_back(tmpBB);
            AllocaPointerVal = LInst;
        }

        if(OnlyUsedInOneBlock)
        {
            if(!OnlyOneBk)
                OnlyOneBk = tmpBB;
            else if(OnlyOneBk != tmpBB)   // 这个是精华  判断这个load 和 store 是否都再一个基本块中
                OnlyUsedInOneBlock = false;
            else 
                continue;
        }
    }
    // 到这里任然存在问题，有待去解决
    // if(BasicBlocknums <= 1)
    // {
    //     OnlyUsedInOneBlock = true;
    // }
}

int BlockInfo:: GetInstIndex(Instruction* Inst)
{
    auto It = InstNumbersIndex.find(Inst);
    if(It != InstNumbersIndex.end())
        return It->second;
    
    int num = 0;
    
}


void PromoteMem2Reg::RemoveFromAList(unsigned& AllocaNum)
{
    Allocas[AllocaNum] = Allocas.back();
    Allocas.pop_back();
    AllocaNum--;
}

bool PromoteMem2Reg::rewriteSingleStoreAlloca(AllocaInfo& info,AllocaInst *AI,  BlockInfo& BBInfo)
{
    StoreInst* OnlySInst = info.OnlyStoreInst;
    bool GlobalVal = false;
    int StoreIndex = -1;

    Value *value = OnlySInst->GetOperand(0);
    User * user = dynamic_cast<User*> (value);  // 如果是const的变量会转换失败的

    if(user == nullptr)
        GlobalVal = true;
    BasicBlock* storeBB = OnlySInst->GetParent();

    // 现在清除是为了之后的添加
    info.UsingBlocks.clear();
    
    for(Use* use :AI->GetValUseList())
    {
        User* user = use->GetUser();
        LoadInst* LInst = dynamic_cast<LoadInst*> (user);
        if(!LInst)
            continue;

        // 我们需要知道 store 和 load 指令的先后关系 Binfo
        if(!GlobalVal)
        {   
            if(LInst->GetParent() == storeBB)
            {
                // Instruction* hello;
                // Instruction* hello = dynamic_cast<Instruction*>(OnlySInst);
                // auto inst = dynamic_cast<Instruction*>(OnlySInst);
                if(StoreIndex == -1){
                    // OnlySInst* 时 StoreInst 指令，继承自 Instruction，tmd为啥不能隐式类型转换呢 ??
                    StoreIndex = BBInfo.GetInstIndex(OnlySInst);
                }
                // OnlySInst* 时 LoadInst 指令，继承自 Instruction，tmd为啥不能隐式类型转换呢 ??? 我迟一点研究一下这个问题
                int LoadIndex = BBInfo.GetInstIndex(LInst);
                if(LoadIndex < StoreIndex){

                }
            }
        }
    }

}


void PromoteMem2Reg::removeLifetimeIntrinsicUsers(AllocaInst* AI)
{
    for(auto UI = AI->GetValUseList().begin(),UE = AI->GetValUseList().end(); UI!=UE;)
    {
        Instruction* inst = dynamic_cast<Instruction*> ((*UI)->GetUser());
        ++UI;
        if(dynamic_cast<LoadInst*> (inst) || dynamic_cast<StoreInst*>(inst))
            continue;

        if ((inst->GetType())->GetTypeEnum() != IR_Value_VOID)
        {
            for (auto UUI = inst->GetValUseList().begin(), UUE = inst->GetValUseList().end(); UUI != UUE;)
            {
                Instruction *AInst = dynamic_cast<Instruction*>((*UUI)->GetUser());
                ++UUI;
                delete AInst;
                // AInst->
            }
        }
        delete inst;
        // I->eraseFromParent();
    }
}

bool PromoteMem2Reg::promoteMemoryToRegister(DominantTree* tree,Function *func,std::vector<AllocaInst *>& Allocas)
{
    AllocaInfo Info;
    BlockInfo BkInfo;
    // 移除没有users 的 alloca指令
    for(unsigned AllocaNum = 0; AllocaNum != Allocas.size(); ++AllocaNum){
        AllocaInst* AI = Allocas[AllocaNum];
        // ？？？
        // removeLifetimeIntrinsicUsers(AI);

         // 移除没有users 的 alloca指令
        if(!AI->isUsed()){
            delete AI;
            RemoveFromAList(AllocaNum);
            // continue; // 可有可无
        }
        // 到接下来为止，我需要记录一些信息
        //例如：记录alloca的定义个数  在不在同一个基本块 支配的信息

        Info.AnalyzeAlloca(AI);
        // 开始分析
        if(Info.DefBlocks.size() == 1) // 仅仅只有一个定义的基本块
        {
            if(rewriteSingleStoreAlloca(Info,AI,BkInfo)) 
            {
                RemoveFromAList(AllocaNum);
                //continue; 这里我确实不理解
            }
        }

        if(Info.OnlyUsedInOneBlock)
        {
            RemoveFromAList(AllocaNum);
        }

        // 
    }

    // Rename 


    return true;
}

void Mem2reg::run()
{
    if(Allocas.empty())
    {
        std::cout << "Allocas is empty" << std::endl;
        return;
    }
    // 通过构造临时对象去执行优化
    bool value =PromoteMem2Reg::promoteMemoryToRegister(_tree,_func,Allocas);
    if(!value)
        std::cout << "promoteMemoryToRegister failed "<<std::endl; 
}

// could be Promoteable?  M-> R alloca 指令
bool PromoteMem2Reg::isAllocaPromotable(AllocaInst* AI)
{
    // Only allow direct and non-volatile loads and stores...  llvm 原话 但是 .sy语言恐怕是没有volatile关键字
    // ValUseList& listPtr = AI->GetValUseList();
    // Use* use --->  ValUseList 
    for(Use* use : AI->GetValUseList())  // value -> use -> user
    {
        User* user = use->GetUser();
        if(LoadInst* LInst = dynamic_cast<LoadInst*> (user))
        {
            assert(LInst);
        }
        else if(StoreInst* SInst = dynamic_cast<StoreInst*> (user))
        {
            // 这种情况是将地址进行存储的情况  store 语句的特点是仅仅只有一个 Use 
            if(SInst->GetOperand(0) == AI)   // user -> use -> value
                return false;
        }
        else if(GepInst* GInst = dynamic_cast<GepInst*> (user))
        {
            // 可以与else 归结到一起
            // 其实这个判断可有可无
            return false;
        }
        else  { return false; }
    }
    return true;
}

