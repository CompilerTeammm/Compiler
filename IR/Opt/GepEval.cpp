#include "../../include/IR/Opt/GepEval.hpp"

bool GepEvaluate::run()
{
    DomTree = AM.get<DominantTree>(func);
    AM.get<SideEffect>(&Singleton<Module>());
    bool modified = false;

    Mapping.clear();
    std::deque<HandleNode *> WorkList;
    BasicBlock *entryBB = func->GetFront();
    auto *rootNode = DomTree->getNode(entryBB);
    WorkList.push_back(new HandleNode(
        DomTree,
        rootNode,
        rootNode->idomChild.begin(),
        rootNode->idomChild.end(),
        std::unordered_map<Value *, std::unordered_map<size_t, Value *>>()
    ));
    Mapping[entryBB] = WorkList.back();
    while (!WorkList.empty())
    {
        GepEvaluate::HandleNode *cur = WorkList.back();
        if (!cur->isProcessed())
        {
            modified |= ProcessNode(cur);
            cur->Process();
        }
        else if (cur->Child() != cur->EndIter())
        {
            auto childNodePtr = *cur->NextChild();
            WorkList.push_back(new HandleNode(
                DomTree,
                childNodePtr,
                childNodePtr->idomChild.begin(),
                childNodePtr->idomChild.end(),
                cur->ValueAddr
            ));
            Mapping[childNodePtr->curBlock] = WorkList.back();
        }
        else
            WorkList.pop_back();
    }
    while (!wait_del.empty())
    {
        Instruction *inst = wait_del.back();
        wait_del.pop_back();
        inst->ClearRelation();
        inst->EraseFromManager();
    }
    return modified;
}

bool GepEvaluate::ProcessNode(HandleNode *node)
{
    bool modified = false;
    BasicBlock *block = node->GetBlock();
    HandleBlockIn(node->ValueAddr, node);
    for (Instruction *inst : *block)
    {
        if (auto alloca = dynamic_cast<AllocaInst *>(inst))
        {
            allocas.insert(alloca);
            continue;
        }
        if (dynamic_cast<CallInst *>(inst))
        {
            if (inst->GetUserUseList()[0]->usee->GetName() == "llvm.memcpy.p0.p0.i32")
            {
                auto array = dynamic_cast<Var *>(inst->GetUserUseList()[2]->usee);
                if (array && array->usage == Var::Constant)
                {
                    if (auto alloca = dynamic_cast<AllocaInst *>(inst->GetUserUseList()[1]->usee))
                    {
                        if (array->GetUserUseList().size() != 0)
                        {
                            auto init = dynamic_cast<Initializer *>(array->GetUserUseList()[0]->usee);
                            AllocaInitMap[alloca] = init;
                            HandleMemcpy(alloca, init, node, std::vector<int>());
                        }
                    }
                }
                continue;
            }
            for (auto &use : inst->GetUserUseList())
            {
                if (use->usee->GetType()->GetTypeEnum() == IR_DataType::IR_PTR)
                {
                    if (auto gep = dynamic_cast<GepInst *>(use->usee))
                    {
                        auto all_offset_zero = [&gep]() {
                            return std::all_of(gep->GetUserUseList().begin() + 1, gep->GetUserUseList().end(),
                                               [](User::UsePtr &use) {
                                                   if (auto val = dynamic_cast<ConstantData *>(use->usee))
                                                   {
                                                       return val->isConstZero();
                                                   }
                                                   return false;
                                               });
                        };
                        if (all_offset_zero() && dynamic_cast<AllocaInst *>(gep->GetUserUseList()[0]->usee))
                        {
                            auto alloca = dynamic_cast<AllocaInst *>(gep->GetUserUseList()[0]->usee);
                            node->ValueAddr[alloca].clear();
                            alloca->AllZero = false;
                            alloca->HasStored = true;
                        }
                    }
                    else if (auto alloca = dynamic_cast<AllocaInst *>(use->usee))
                    {
                        node->ValueAddr[alloca].clear();
                        alloca->AllZero = false;
                        alloca->HasStored = true;
                    }
                }
                else if (auto alloca = dynamic_cast<AllocaInst *>(use->usee))
                {
                    node->ValueAddr[alloca].clear();
                    alloca->AllZero = false;
                    alloca->HasStored = true;
                }
            }
            continue;
        }
        if (dynamic_cast<LoadInst *>(inst))
        {
            if (auto gep = dynamic_cast<GepInst *>(inst->GetUserUseList()[0]->usee))
            {
                if (auto alloca = dynamic_cast<AllocaInst *>(gep->GetUserUseList()[0]->usee))
                {
                    size_t hash = GepHash{}(gep, &node->ValueAddr);
                    if (node->ValueAddr.find(alloca) != node->ValueAddr.end())
                    {
                        if (node->ValueAddr[alloca].find(hash) != node->ValueAddr[alloca].end())
                        {
                            inst->ReplaceAllUseWith(node->ValueAddr[alloca][hash]);
                            wait_del.push_back(inst);
                            modified = true;
                        }
                    }
                    else if (!alloca->HasStored)
                    {
                        bool flag_all_const = true;
                        std::vector<int> index;
                        for (int i = 2; i < gep->GetUserUseList().size(); i++)
                        {
                            if (auto INT = dynamic_cast<ConstIRInt *>(gep->GetUserUseList()[i]->usee))
                                index.push_back(INT->GetVal());
                            else
                            {
                                flag_all_const = false;
                                break;
                            }
                        }
                        if (auto init = AllocaInitMap[alloca])
                        {
                            if (flag_all_const)
                            {
                                if (auto val = init->GetInitVal(index))
                                {
                                    inst->ReplaceAllUseWith(val);
                                    wait_del.push_back(inst);
                                    modified = true;
                                }
                            }
                        }
                    }
                }
            }
            continue;
        }
        if (dynamic_cast<StoreInst *>(inst))
        {
            if (auto gep = dynamic_cast<GepInst *>(inst->GetUserUseList()[1]->usee))
            {
                if (auto alloca = dynamic_cast<AllocaInst *>(gep->GetUserUseList()[0]->usee))
                {
                    alloca->HasStored = true;
                    alloca->AllZero = false;
                    AllocaInitMap.erase(alloca);
                    auto all_offset_const = [&gep]() {
                        return std::all_of(gep->GetUserUseList().begin() + 1, gep->GetUserUseList().end(),
                                           [](User::UsePtr &use) {
                                               if (auto val = dynamic_cast<ConstantData *>(use->usee))
                                               {
                                                   return true;
                                               }
                                               return false;
                                           });
                    };
                    if (!all_offset_const())
                        node->ValueAddr[alloca].clear();
                    size_t hash = GepHash{}(gep, &node->ValueAddr);
                    node->ValueAddr[alloca][hash] = inst->GetUserUseList()[0]->usee;
                }
                else if (auto gep_base = dynamic_cast<GepInst *>(gep->GetUserUseList()[0]->usee))
                {
                    assert(0 && "GepCombine has not been run");
                    if (auto alloca = dynamic_cast<AllocaInst *>(gep->GetUserUseList()[0]->usee))
                    {
                        size_t hash = GepHash{}(gep, &node->ValueAddr);
                        node->ValueAddr[alloca][hash] = inst->GetUserUseList()[0]->usee;
                    }
                }
                else if (gep->GetUserUseList()[0]->usee->isParam())
                {
                    auto base = gep->GetUserUseList()[0]->usee;
                    auto all_offset_const = [&gep]() {
                        return std::all_of(gep->GetUserUseList().begin() + 1, gep->GetUserUseList().end(),
                                           [](User::UsePtr &use) {
                                               if (auto val = dynamic_cast<ConstantData *>(use->usee))
                                               {
                                                   return true;
                                               }
                                               return false;
                                           });
                    };
                    if (!all_offset_const())
                        node->ValueAddr[base].clear();
                    size_t hash = GepHash{}(gep, &node->ValueAddr);
                    node->ValueAddr[base][hash] = inst->GetUserUseList()[0]->usee;
                }
            }
            continue;
        }
    }
    return modified;
}

void GepEvaluate::HandleMemcpy(AllocaInst *inst, Initializer *init, HandleNode *node, std::vector<int> index)
{
    if (init->size() == 0)
    {
        inst->AllZero = true;
        return;
    }
    int limi = dynamic_cast<ArrayType *>(init->GetType())->GetNum();
    for (int i = 0; i < limi; i++)
    {
        if (i < init->size())
        {
            index.push_back(i);
            if (auto inits = dynamic_cast<Initializer *>((*init)[i]))
            {
                HandleMemcpy(inst, inits, node, index);
                index.pop_back();
            }
            else
            {
                node->ValueAddr[inst][InitHash{}(inst, index)] = (*init)[i];
                index.pop_back();
            }
        }
        else 
        {
            index.push_back(i);
            HandleZeroInitializer(inst, node, index);
            index.pop_back();
        }
    }
}

void GepEvaluate::HandleZeroInitializer(AllocaInst *inst, HandleNode *node, std::vector<int> index)
{
    return; // TODO
}

void GepEvaluate::HandleBlockIn(ValueAddr_Struct &addr, HandleNode *node)
{
    if (node->GetBlock()->GetValUseList().GetSize() < 2)
        return;
    std::vector<ValueAddr_Struct *> maps;

    for (auto user : node->GetBlock()->GetValUseList())
    {
        Instruction *inst = static_cast<Instruction *>(user->GetUser());
        BasicBlock *userblock = inst->GetParent();
        auto HandleNode = Mapping[userblock];
        if (HandleNode)
            maps.push_back(&(HandleNode->ValueAddr));
        else
        {
            for (auto alloca : allocas)
                alloca->HasStored = true;
            addr.clear();
            return;
        }
    }

    auto findCommonKeys = [](std::vector<ValueAddr_Struct *> &maps) {
        std::unordered_set<Value *> allKeys;
        std::unordered_set<Value *> commonKeys;

        for (auto &map : maps)
        {
            for (auto &[key, value] : *map)
            {
                if (key && allKeys.count(key))
                {
                    commonKeys.insert(key); 
                }
                else
                {
                    allKeys.insert(key);
                }
            }
        }
        return commonKeys;
    };
    auto commonKeys = findCommonKeys(maps);
    if (!commonKeys.empty())
    {
        for (auto key : commonKeys)
        {
            if (addr.find(key) != addr.end())
                addr.erase(key);
        }
    }
}