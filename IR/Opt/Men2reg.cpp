#pragma once
#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "../../include/IR/Opt/Men2reg.hpp"
#include <memory>
#include <set>
#include"IDF.hpp"

// walk along the logic 
//Todo: DomTree Pre DFS_order sdom->idom  DF 
//  IDF iterate DF 
// Pass ReName
// Simply Inst 


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
            // AllocaPointerVal = SInst->GetOperand(0);
            DefBlocks.push_back(tmpBB);
            // BasicBlocknums++;  说实话，这个鸡肋了，因为 defBlocks就可以求出里面的个数
            OnlyStoreInst =SInst;
        }
        else{
            LoadInst* LInst = dynamic_cast<LoadInst*>(user);
            tmpBB = LInst->GetParent();
            // BasicBlock* parent = nullptr; //临时测试用的
            UsingBlocks.push_back(tmpBB);
            // AllocaPointerVal = LInst;
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

// There are blocks which lead to uses,void inserting PHI nodes int the block we don t lead to uses
void PromoteMem2Reg::ComputeLiveInBlocks(AllocaInst* AI,std::set<BasicBlock*>& DefBlock,
                        std::set<BasicBlock*>& LiveInBlocks,AllocaInfo& info)
{
    // we must iterate through the predecessors of blocks where the def is live
    // %a = alloca i32; (store i32 10,i32* %a;) %val = load i32,i32* %a 
    // 这个计算的核心在于查找use（load）的基本块里面，val值是不是活跃的

    // workList 
    std::vector<BasicBlock*> WorkLiveIn(info.UsingBlocks.begin(),
                                        info.UsingBlocks.end());
    for(int i = 0, e = WorkLiveIn.size(); i != e;i++)
    {
        BasicBlock* BB =WorkLiveIn[i];
        if(!DefBlock.count(BB)) 
            continue; //判断有没有store的情况
        
        for(auto it = BB->end();;++it)
        {
            Instruction* inst = *it;
            // 最开始是storeInst的处理
            if(StoreInst* SInst = dynamic_cast<StoreInst*> (inst)){
                if(SInst->GetOperand(1) != AI)
                    continue;
                
                WorkLiveIn[i] = WorkLiveIn.back();
                WorkLiveIn.pop_back();
                --i;
                --e;
                break;
            }

            if(LoadInst* LInst = dynamic_cast<LoadInst*>(inst))
            {
                if(LInst->GetOperand(0) == AI)
                    break;
            }
        }
    }

    while(!WorkLiveIn.empty()){
        BasicBlock* BB = WorkLiveIn.back();
        WorkLiveIn.pop_back();

        if(!LiveInBlocks.insert(BB).second)
            continue;
        // 接下来是for循环的对前驱的遍历，需要建立支配关系，
        // 寻找前驱是store的块，才可以终止对pre的回溯

        // 需要支配树，但是支配树我还没有实现，需要支配树提供前驱节点
        for(auto& pre :BB)
        {
            if(DefBlock.count(pre))
                continue;
            
            WorkLiveIn.push_back(pre);
        }
        
    }

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

// 创建队列去插入phi函数，建立<PHI*,int>的map关系
// Todo: PhiInst 指令我还没有实现它
bool PromoteMem2Reg::QueuePhiNode(BasicBlock* BB, int AllocaNum)
{
    // 为了类型匹配我进行了转换
    auto newBB = std::unique_ptr<BasicBlock> (BB);
    PhiInst* &PN = NewPhiNodes[std::make_pair(BBNumbers[newBB],AllocaNum)];
    
    //  had  a phi func
    if(PN)
        return false;
    
    // PhiInst 我还没用实现
    int num;
    BasicBlock* BB1;
    PN =PhiInst::Create(Allocas[AllocaNum]->GetType(),num,
                        Allocas[AllocaNum]->GetName()+ ".",
                         BB1);
    PhiToAllocaMap[PN] = AllocaNum;

    return true;
}


void PromoteMem2Reg::RemoveFromAList(int& AllocaNum)
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
        // 未实现
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

        auto it = std::lower_bound(StoreByIndex.begin(),StoreByIndex.end(),
                        static_cast<StoreInst*>(nullptr),less_first());
        // LoadIndex < StoreIndex
        if(it == StoreByIndex.begin())
        {
            if(StoreByIndex.empty())  // 没有storeInst 的情况，undefValue出场
                LInst->ReplaceAllUseWith(UndefValue::Get(LInst->GetType()));
            else   // 没有的话，这条优化无法执行 there is no store before this load;
                return false;
        }
        else 
            LInst->ReplaceAllUseWith(std::prev(it)->second->GetOperand(0));
        delete LInst;
        BkInfo.DeletIndex(LInst);
    }
    
    // remove the dead stores and alloca
    for(Use* use:AI->GetValUseList()){
        assert(dynamic_cast<StoreInst*>(use->GetUser()) && " should be a SInst,LInst is deleted");
        StoreInst * SInst = dynamic_cast<StoreInst*> (use->GetUser());
        delete SInst;
        BkInfo.DeletIndex(SInst);
    }

    delete  AI;
    BkInfo.DeletIndex(AI);

    return true;
}

// knowing that the alloca is promotable,we know that it is safe 
//to kill all insts except for load and store
void PromoteMem2Reg::removeLifetimeIntrinsicUsers(AllocaInst* AI)
{
    for(auto UI = AI->GetValUseList().begin(),UE = AI->GetValUseList().end(); UI!=UE;)
    {
        Instruction* inst = dynamic_cast<Instruction*> ((*UI)->GetUser());
        ++UI;
        if(dynamic_cast<LoadInst*> (inst) || dynamic_cast<StoreInst*>(inst))
            continue;

        // is good for dead code elimination later
        // 产生了一个值，这个值是 lifetime intrinsic 
        if ((inst->GetType())->GetTypeEnum() != IR_Value_VOID)
        {
            // bitcast/GEP
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
    IDFCalculator Idf;

    // 移除没有users 的 alloca指令
    for(int AllocaNum = 0; AllocaNum != Allocas.size(); ++AllocaNum){
        AllocaInst* AI = Allocas[AllocaNum];
        
        // 净化IR，使得只保留那些对程序真正有影响的内存操作
        removeLifetimeIntrinsicUsers(AI);

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
            // rewrite 和 promote 函数内部进行了 delete
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
        

        // computed a numbering for the BB's in the func
        if(BBNumbers.empty())
        {
            int ID = 0;
            for(auto &BB :_func->GetBBs())
                BBNumbers[BB] = ID++;
        }

        // keep the reverse mapping of the 'Allocas' array for the rename pass
        AllocaLookup[Allocas[AllocaNum]] = AllocaNum;
        
        // 这个是defBlock的构造  store指令
        std::set<BasicBlock*> DefineBlock(Info.DefBlocks.begin(),Info.DefBlocks.end());

        std::set<BasicBlock*> LiveInBlocks;

        //输出型参数，我得到 LiveInBlocks 计算活跃性的
        ComputeLiveInBlocks(AI,DefineBlock,LiveInBlocks,Info);


        //////// determine which block nodes need phi functions
        std::vector<BasicBlock*> PhiBlocks;

        // 就是为了calculate做准备d
        Idf.setDefiningBlocks(DefineBlock);
        Idf.setLiveInBlocks(LiveInBlocks);
        //迭代支配边界
        Idf.calculate(PhiBlocks);
        // 到这里应该 PhiBlocks 已经被构建完成了

        // 排序以据可以是DFS的遍历顺序
        /// 然后对基本块根据序号进行排序，使得插入phi指令的顺序和编号确定化
        if(PhiBlocks.size() > 1)
            std::sort(PhiBlocks.begin(),PhiBlocks.end(),
                     [this](std::unique_ptr<BasicBlock>& A,std::unique_ptr<BasicBlock>& B){
                            return BBNumbers.at(A) < BBNumbers.at(B);
                     });

        // 到这里为止，我应该是拥有了改插入phi的节点了
        for(int i = 0,e = PhiBlocks.size();i !=e; i++){
            QueuePhiNode(PhiBlocks[i],AllocaNum);
        }
    }

    if(Allocas.empty())
        return;

    //Rename
    // 这个倒是好实现，作用啥的我仔细看看
    // BkInfo.clear();

    // 初始化ValVector，
    RenamePassData::ValVector Values(Allocas.size());
    for(int i = 0, e =Allocas.size(); i!=e; ++i)
        Values[i] = UndefValue::Get(Allocas[i]->GetType());
    
    /// WorkList 主要是
    std::vector<RenamePassData> RenamePasWorkList;
    // 这里sb报错，不知道为啥
    RenamePasWorkList.emplace_back(_func->begin(),nullptr,std::move(Values));
    do{
        auto tmp = std::move(RenamePasWorkList.back());
        RenamePasWorkList.pop_back();
        // 一个核心逻辑
        // ReName();
    }while(!RenamePasWorkList.empty());

    ///////// 下面是重命名之后的额外操作

    for(int i = 0,e=Allocas.size(); i!=e;i++)
    {
        Instruction* A =Allocas[i];

        if(!A->is_empty())
            A->ReplaceAllUseWith(UndefValue::Get(A->GetType()));
        delete A;
    }

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

