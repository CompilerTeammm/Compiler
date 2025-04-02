
#include "../../include/IR/Opt/MemoryToRegister.hpp"
#include "Passbase.hpp"
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
    clear();
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
            dynamic_cast<AllocaInst *>((*Inst)->GetUserUseList()[0]->usee) ||
        dynamic_cast<StoreInst *>(*Inst) &&
            dynamic_cast<AllocaInst *>((*Inst)->GetUserUseList()[1]->usee));
}

// There are blocks which lead to uses,void inserting PHI nodes int the block we don t lead to uses

void PromoteMem2Reg::ComputeLiveInBlocks(AllocaInst *AI, std::set<BasicBlock *> &DefBlock,
                         std::set<BasicBlock *> &LiveInBlocks, AllocaInfo &info)
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
        
        for(auto it = BB->begin();;++it)
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
                if(LInst->GetOperand(0) != AI)
                    continue;
                
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

        // 支配树搭建完成，取BB的前驱结点
        
        for(auto& preNode :_tree->getNode(BB)->predNodes)
        {
            BasicBlock* preBB = preNode->curBlock;
            if(DefBlock.count(preBB))
                continue;
            
            WorkLiveIn.push_back(preBB);
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
    // auto newBB = std::shared_ptr<BasicBlock> (BB);
    PhiInst* &PN = NewPhiNodes[std::make_pair(BBNumbers[BB],AllocaNum)];
    
    // if the BB already has a phi node , return false
    if(PN)
        return false;
    
    AllocaInst* AInst = Allocas[AllocaNum];
    // PhiInst 我还没用实现
    PN =PhiInst::Create(BB->GetFront(), BB, 
                        dynamic_cast<HasSubType*>(AInst->GetType())->GetSubType());
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
        LoadInst* LInst = dynamic_cast<LoadInst*> (AIuser);
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
            } else if( LInst->GetParent() != StoreBB  // 不在同一个BB中    一个支配关系 
            && !_tree->dominates(StoreBB, LInst->GetParent()))
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

        // auto it = std::lower_bound(StoreByIndex.begin(),StoreByIndex.end(),
        //                 static_cast<StoreInst*>(nullptr),less_first());
        auto it = std::lower_bound(StoreByIndex.begin(), StoreByIndex.end(),
                                   std::make_pair(LoadIndex, static_cast<StoreInst *>(nullptr)),
                                   less_first());
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
void PromoteMem2Reg::removeLifetimeIntrinsicUsers(AllocaInst *AI)
{
    for (auto UI = AI->GetValUseList().begin(), UE = AI->GetValUseList().end(); UI != UE;)
    {
        Instruction *inst = dynamic_cast<Instruction *>((*UI)->GetUser());
        ++UI;
        if (dynamic_cast<LoadInst *>(inst) || dynamic_cast<StoreInst *>(inst))
            continue;

        // is good for dead code elimination later
        // 产生了一个值，这个值是 lifetime intrinsic
        if ((inst->GetType())->GetTypeEnum() != IR_Value_VOID)
        {
            // bitcast/GEP
            for (auto UUI = inst->GetValUseList().begin(), UUE = inst->GetValUseList().end(); UUI != UUE;)
            {
                Instruction *AInst = dynamic_cast<Instruction *>((*UUI)->GetUser());
                ++UUI;
                delete AInst;
                // AInst->
            }
        }
        delete inst;
        // I->eraseFromParent();
    }
}


// One way is to using stack to store the value, DFS 遍历， replace , delete

// RenamPassWorkList add  {BB = entry,Pred = nullptr,
//  IncomingVals = {Undef ... ...}  }
// IncomingVals  <->  Allocas
// store --> IncomingVals   load   <-- IncomingVals
void PromoteMem2Reg::RenamePass(BasicBlock *BB, BasicBlock *Pred,
                                RenamePassData::ValVector &IncomingVals,
                                std::vector<RenamePassData> &WorkList){
    // llvm 中使用 goto 实现，有点意思

    // 这个填充phi函数的逻辑，我怎么感觉就是一坨大的
    while(true)
    {   
        if(PhiInst* PhiInt = dynamic_cast<PhiInst*>((*BB->begin())))
        {
            if(PhiToAllocaMap.count(PhiInt))
            {
                for(auto Inst = BB->begin(); Inst != BB->end();)
                {
                    if(PhiToAllocaMap.count(PhiInt))
                    {
                        // 这个逻辑我感觉有大问题吧？？？？
                        int AllocaNum = PhiToAllocaMap[PhiInt];
                        // add value
                        PhiInt->addIncoming(IncomingVals[AllocaNum],Pred);
                        IncomingVals[AllocaNum] = PhiInt;
                        ++Inst;
                        PhiInt = dynamic_cast<PhiInst*>(*Inst);
                        if(!PhiInt)
                            break;
                    }
                    else {
                        break;
                    }
                }
            }
        }


        // set是一个去重容器，插入返回一个pair 类型的值，第二个值是bool值，表示插入成功与否
        // auto it = Visited.insert(BB);
        if(!Visited.insert(BB).second)
            return;

        // 遍历BB 中的语句
        for(auto inst = BB->begin();inst != BB->end();){
            Instruction* I = *inst;
            ++inst;

            if(LoadInst* LI = dynamic_cast<LoadInst*>(I)) 
            {
                // 不能是对 alloca 地址的操作
                auto op = LI->GetOperand(0);
                AllocaInst* AI = dynamic_cast<AllocaInst*> (op);
                if(!AI)
                    continue;
                
                auto it = AllocaLookup.find(AI);
                if(it == AllocaLookup.end())
                    continue;
                
                // alloca and IncomingVals one to one
                Value* V = IncomingVals[it->second];

                LI->ReplaceAllUseWith(V);
                delete LI;
            }
            else if(StoreInst* SI = dynamic_cast<StoreInst*>(I)){
                // 同样 store 指令对其地址的操作也不可以被优化
                auto &des = SI->GetUserUseList();
                Value* Des = des[1]->GetValue(); //取第二个操作数 ？？？
                AllocaInst* AI = dynamic_cast<AllocaInst*>(Des);
                if(!AI)
                    continue;
                
                auto it = AllocaLookup.find(AI);
                if(it == AllocaLookup.end())
                    continue;
                
                IncomingVals[it->second] = SI->GetOperand(0);
                delete SI;
            }
        }

        auto cur = _tree->getNode(BB);
        if(cur->succNodes.empty())
            return;
        
        // keep track of the successor so we dont visit the same successor twice
        std::set<BasicBlock*> VisitedSucc;
        VisitedSucc.insert(BB);

        auto It = cur->succNodes.begin();
        Pred = BB;

        // BB = _tree.getNode(*It++).curBlock;

        BB = (*It)->curBlock;
        It++;

        for(;It != cur->succNodes.end();It++)
        {
            // 把未处理的结点，全部塞进去
            BasicBlock* tmp = (*It)->curBlock;
            if(VisitedSucc.insert(tmp).second)  // 前驱都是不变的
                WorkList.emplace_back(tmp,Pred,IncomingVals);
        }

    }
}

bool PromoteMem2Reg::promoteMemoryToRegister()
{
    AllocaInfo Info;
    BlockInfo BkInfo;
    IDFCalculator Idf(*_tree);

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
                BBNumbers[BB.get()] = ID++;
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

        // 就是为了calculate做准备
        Idf.setDefiningBlocks(DefineBlock);
        Idf.setLiveInBlocks(LiveInBlocks);
        //迭代支配边界
        Idf.calculate(PhiBlocks);
        // 到这里应该 PhiBlocks 出来了

        // 排序就是Fuction中BBs的顺序
        ///使得插入phi指令的顺序和编号确定化
        if(PhiBlocks.size() > 1)
            std::sort(PhiBlocks.begin(),PhiBlocks.end(),
                     [this](BasicBlock* A,BasicBlock* B){
                            return BBNumbers.at(A) < BBNumbers.at(B);
                     });

        // 到这里为止，我应该是拥有了该插入phi的节点了
        for(int i = 0, e = PhiBlocks.size(); i !=e; i++){
            QueuePhiNode(PhiBlocks[i],AllocaNum);
        }
    }

    if(Allocas.empty())
        return true;

    //Rename
    BkInfo.clear();

    // 初始化ValVector，
    RenamePassData::ValVector Values(Allocas.size());
    for(int i = 0, e =Allocas.size(); i!=e; ++i)
        Values[i] = UndefValue::Get(Allocas[i]->GetType());
    
    /// WorkList 主要是
    std::vector<RenamePassData> RenamePassWorkList;
    
    //隐式类型的转换  
    // WorkList最开始就是把 entry放进去
    RenamePassWorkList.emplace_back(*(_func->begin()),nullptr,std::move(Values));
    do{
        auto tmp = std::move(RenamePassWorkList.back());
        RenamePassWorkList.pop_back();

        // core function for rename
        // transfer WorkList 
        RenamePass(tmp.BB,tmp.PredBB,tmp.Values,RenamePassWorkList);
    }while(!RenamePassWorkList.empty());

    for(int i = 0,e=Allocas.size(); i!=e;i++)
    {
        Instruction* A =Allocas[i];
        // They must be in unreachable basic blocks.
        // Just delete the user now
        if(A->is_empty())
            delete A;
    }

    // I think the deleted phi will affect the value,so we need while
    bool IsEliminated = true;
    while(IsEliminated){
        IsEliminated = false;
        
        // Iterating over NewPhiNodes is deterministic, so it is safe to simplify and RAUW
        // If the phiInst merges one value and/or undefs,get the value
        for(auto I = NewPhiNodes.begin(),E = NewPhiNodes.end(); I !=E ; I++)
        {
            PhiInst* PInst = I->second;
            if(PInst->IsReplaced())
            {
                Value* value = PInst->GetOperand(0);
                PInst->ReplaceAllUseWith(value);
                NewPhiNodes.erase(I++);
                IsEliminated = true;
            }
        }   
    }

    // ++i  or  i++
    // c u     u c
    // this is to deal some phiInst which unreachable
    for(auto I = NewPhiNodes.begin(), 
             E = NewPhiNodes.end();  I != E; ++I){
        
        PhiInst* SomePHI = I->second;
        BasicBlock* BB = SomePHI->GetParent();
        if(BB->GetFront() != SomePHI)
            continue;
        
        // 这个函数要在phiInst里面实现
        int  BBnum = _tree->getNode(BB)->predNodes.size(); 
        if(SomePHI->getNumIncomingValues() == BBnum )
            continue;
        
        std::vector<BasicBlock*> Preds;
        for(auto e : _tree->getNode(BB)->predNodes)
        {
            Preds.push_back(e->curBlock);
        }
        // 方便二分查找
        std::sort(Preds.begin(),Preds.end());

        for(int i = 0, e = SomePHI->getNumIncomingValues(); i !=e; i++)
        {
            auto it = std::lower_bound(Preds.begin(), Preds.end(),
                             SomePHI->getIncomingBlock(i));
            Preds.erase(it);
        }

        int NumBadPreds = SomePHI->getNumIncomingValues();
        auto BBI = BB->begin();
        while ((SomePHI = dynamic_cast<PhiInst*>(*BBI)) &&
               SomePHI->getNumIncomingValues() == NumBadPreds)
        {
            Value *UndefVal = UndefValue::Get(SomePHI->GetType());
            for (unsigned pred = 0, e = Preds.size(); pred != e; ++pred)
                SomePHI->addIncoming(UndefVal, Preds[pred]);
        }
    }

    NewPhiNodes.clear();
    return true;
}

