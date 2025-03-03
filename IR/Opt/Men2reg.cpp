#pragma once
#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "../../include/IR/Opt/Men2reg.hpp"
// pair  仿函数  去给排序用的
struct less_first {
    template<typename T> bool operator()(const T& lhs, const T& rhs) const{
        return lhs.first < rhs.first;
    }
};

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

bool BlockInfo::isInterestingInstruction(List<BasicBlock, Instruction>::iterator Inst)
{
    return (dynamic_cast<LoadInst *>(*Inst) &&
            dynamic_cast<AllocaInst *>((*Inst)->Getuselist()[0]->usee) ||
        dynamic_cast<StoreInst *>(*Inst) &&
            dynamic_cast<AllocaInst *>((*Inst)->Getuselist()[1]->usee));
}


// 确定load 和 store 指令的先后顺序
int BlockInfo:: GetInstIndex(Instruction* Inst)
{
    // 建立好了map之后的事情了这是
    auto iterator = InstNumbersIndex.find(Inst);
    if(iterator != InstNumbersIndex.end())
        return iterator->second;
    
    int num = 0;
    BasicBlock* BB = Inst->GetParent();
    for(auto inst = BB->begin();inst !=BB->end();++inst){
        if(isInterestingInstruction(inst))
        {
            InstNumbersIndex[*inst] = num++;
        }
    }

    return InstNumbersIndex[Inst];
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
    int StoreIndex = -1;
    bool GlobalVal = false;

    Value* value = OnlySInst->GetOperand(0);
    User* user = dynamic_cast<User*>( value);
    if( user == nullptr)  // 继承不一样会出现转换失败的情况
        GlobalVal = true;
    BasicBlock* StoreBB = OnlySInst->GetParent();

    info.UsingBlocks.clear();

    for(Use* use : AI->GetValUseList())
    {
        User* AIuser = use->GetUser();
        LoadInst* LInst = dynamic_cast<LoadInst*> (user);
        if(!LInst)  // 只让LoadInst 语句下去
            continue;

        if(!GlobalVal) {
            if(LInst->GetParent() == StoreBB) {  // 这个是和store语句在同一个BB中
                if(StoreIndex == -1) // 如果调试的时候仍然出错，尝试用 dynamic_cast <> 去转换成功它
                    StoreIndex = BBInfo.GetInstIndex(OnlySInst);  // 不理解为什么不可以直接传参数过去，应该是可以发生隐式类型转换的
                int LoadIndex = BBInfo.GetInstIndex(LInst);
                if(LoadIndex < StoreIndex) // undef 去赋值需要
                {
                    info.UsingBlocks.push_back(StoreBB);
                    continue;
                }
            }  // 不在同一个BB中    一个支配关系
            else if( LInst->GetParent() != StoreBB 
            && _tree->dominates(StoreBB, LInst->GetParent()))
            {
                info.UsingBlocks.push_back(LInst->GetParent());
                continue;
            }
        }
        LInst->ReplaceAllUseWith(value);
        delete LInst;
        BBInfo.DeletIndex(LInst);
    }

    if(!info.UsingBlocks.empty())
        return false;
    delete OnlySInst;
    BBInfo.DeletIndex(OnlySInst);
    delete AI;
    BBInfo.DeletIndex(AI);

    return true;
}

bool PromoteMem2Reg::promoteSingleBlockAlloca(AllocaInfo &Info, AllocaInst *AI, BlockInfo &BkInfo)
{
    // 这里的判断已经默认是load 和 store 在同一个BB里面的了
    std::vector<std::pair<int,StoreInst*>> StoreByIndex;

    for(Use* Use : AI->GetValUseList())
    {
        User* user = Use->GetUser();
        if(StoreInst* SInst = dynamic_cast<StoreInst*>(user))
            StoreByIndex.push_back(std::make_pair(BkInfo.GetInstIndex(SInst),SInst));
    }
        // 仿函数的传入， 对StoreByIndex 进行对象的排序
    std::sort(StoreByIndex.begin(),StoreByIndex.end(),less_first());

    // walk all of the loads from this alloca , replacing them with the nearest store above them
    for(Use* use : AI->GetValUseList())
    {
        User* user = use->GetUser();
        LoadInst*LInst = dynamic_cast<LoadInst*>(user);
        if(!LInst) 
            continue;
        
        int LoadIndex = BkInfo.GetInstIndex(LInst);

        std::lower_bound();
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
            continue; // 可有可无
        }
        // 到接下来为止，我需要记录一些信息
        //例如：记录alloca的定义个数  在不在同一个基本块 支配的信息

        Info.AnalyzeAlloca(AI);
        // 开始分析

        //第一个优化
        if(Info.DefBlocks.size() == 1) // 仅仅只有一个定义的基本块
        {
            if(rewriteSingleStoreAlloca(Info,AI,BkInfo)) 
            {
                RemoveFromAList(AllocaNum);
                continue; 
            }
        }

        // 第二个优化                       这里满足store 在 load前面，并且发生了替换
        if(Info.OnlyUsedInOneBlock && promoteSingleBlockAlloca(Info,AI,BkInfo))
        {
            RemoveFromAList(AllocaNum);
            continue;
        }
        // 部分优化执行完成了，该进行对插入phi函数的操作了
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

