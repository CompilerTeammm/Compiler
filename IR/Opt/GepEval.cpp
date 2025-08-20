#include "../../include/IR/Opt/GepEval.hpp"

bool GepEvaluator::processNode(NodeHandler* node)
{
    bool changed = false;
    BasicBlock* blk = node->Block();

    processBlock(node->valueMapping, node);

    for (Instruction* inst : *blk)
    {
        if (auto allocInst = dynamic_cast<AllocaInst*>(inst))
        {
            allocas.insert(allocInst);
            continue;
        }

        if (auto call = dynamic_cast<CallInst*>(inst))
        {
            if (inst->GetUserUseList()[0]->usee->GetName() == "llvm.memcpy.p0.p0.i32")
            {
                auto arrVar = dynamic_cast<Var*>(inst->GetUserUseList()[2]->usee);
                if (arrVar && arrVar->usage == Var::Constant)
                {
                    if (auto allocInst = dynamic_cast<AllocaInst*>(inst->GetUserUseList()[1]->usee))
                    {
                        if (!arrVar->GetUserUseList().empty())
                        {
                            auto init = dynamic_cast<Initializer*>(arrVar->GetUserUseList()[0]->usee);
                            allocInitMap[allocInst] = init;
                            handleMemcpy(allocInst, init, node, {});
                        }
                    }
                }
                continue;
            }

            for (auto& u : inst->GetUserUseList())
            {
                Value* target = u->usee;
                if (target->GetType()->GetTypeEnum() == IR_DataType::IR_PTR)
                {
                    if (auto gep = dynamic_cast<GepInst*>(target))
                    {
                        auto all_zero = [&gep]() {
                            return std::all_of(gep->GetUserUseList().begin() + 1, gep->GetUserUseList().end(),
                                               [](User::UsePtr& use) {
                                                   if (auto val = dynamic_cast<ConstantData*>(use->usee))
                                                       return val->isConstZero();
                                                   return false;
                                               });
                        };
                        if (all_zero() && dynamic_cast<AllocaInst*>(gep->GetUserUseList()[0]->usee))
                        {
                            auto allocInst = dynamic_cast<AllocaInst*>(gep->GetUserUseList()[0]->usee);
                            node->valueMapping[allocInst].clear();
                            allocInst->AllZero = false;
                            allocInst->HasStored = true;
                        }
                    }
                    else if (auto allocInst = dynamic_cast<AllocaInst*>(target))
                    {
                        node->valueMapping[allocInst].clear();
                        allocInst->AllZero = false;
                        allocInst->HasStored = true;
                    }
                }
                else if (auto allocInst = dynamic_cast<AllocaInst*>(target))
                {
                    node->valueMapping[allocInst].clear();
                    allocInst->AllZero = false;
                    allocInst->HasStored = true;
                }
            }
            continue;
        }

        if (auto load = dynamic_cast<LoadInst*>(inst))
        {
            if (auto gep = dynamic_cast<GepInst*>(load->GetUserUseList()[0]->usee))
            {
                if (auto allocInst = dynamic_cast<AllocaInst*>(gep->GetUserUseList()[0]->usee))
                {
                    size_t h = GepHasher{}(gep, &node->valueMapping);
                    if (node->valueMapping.count(allocInst) &&
                        node->valueMapping[allocInst].count(h))
                    {
                        load->ReplaceAllUseWith(node->valueMapping[allocInst][h]);
                        pendingDelete.push_back(load);
                        changed = true;
                    }
                    else if (!allocInst->HasStored)
                    {
                        bool all_const = true;
                        std::vector<int> idxs;
                        for (int i = 2; i < gep->GetUserUseList().size(); ++i)
                        {
                            if (auto c = dynamic_cast<ConstIRInt*>(gep->GetUserUseList()[i]->usee))
                                idxs.push_back(c->GetVal());
                            else
                            {
                                all_const = false;
                                break;
                            }
                        }
                        if (auto init = allocInitMap[allocInst])
                        {
                            if (all_const)
                            {
                                if (auto val = init->GetInitVal(idxs))
                                {
                                    load->ReplaceAllUseWith(val);
                                    pendingDelete.push_back(load);
                                    changed = true;
                                }
                            }
                        }
                    }
                }
            }
            continue;
        }

        if (auto store = dynamic_cast<StoreInst*>(inst))
        {
            if (auto gep = dynamic_cast<GepInst*>(store->GetUserUseList()[1]->usee))
            {
                Value* base = gep->GetUserUseList()[0]->usee;
                AllocaInst* allocInst = dynamic_cast<AllocaInst*>(base);
                if (allocInst)
                {
                    allocInst->HasStored = true;
                    allocInst->AllZero = false;
                    allocInitMap.erase(allocInst);

                    auto offsets_all_const = [&gep]() {
                        return std::all_of(gep->GetUserUseList().begin() + 1, gep->GetUserUseList().end(),
                                           [](User::UsePtr& u) { return dynamic_cast<ConstantData*>(u->usee) != nullptr; });
                    };
                    if (!offsets_all_const())
                        node->valueMapping[allocInst].clear();

                    size_t h = GepHasher{}(gep, &node->valueMapping);
                    node->valueMapping[allocInst][h] = store->GetUserUseList()[0]->usee;
                }
                else if (base->isParam())
                {
                    auto offsets_all_const = [&gep]() {
                        return std::all_of(gep->GetUserUseList().begin() + 1, gep->GetUserUseList().end(),
                                           [](User::UsePtr& u) { return dynamic_cast<ConstantData*>(u->usee) != nullptr; });
                    };
                    if (!offsets_all_const())
                        node->valueMapping[base].clear();

                    size_t h = GepHasher{}(gep, &node->valueMapping);
                    node->valueMapping[base][h] = store->GetUserUseList()[0]->usee;
                }
            }
            continue;
        }
    }

    return changed;
}

void GepEvaluator::handleMemcpy(AllocaInst* alloc, Initializer* init, NodeHandler* node, std::vector<int> idx) 
{
    if (init->size() == 0)
    {
        handleZeroInit(alloc, node, idx); // 统一通过 handleZeroInit 处理
        return;
    }

    if (auto arrayType = dynamic_cast<ArrayType*>(init->GetType()))
    {
        int len = arrayType->GetNum();
        for (int i = 0; i < len; ++i)
        {
            idx.push_back(i);

            if (i < init->size())
            {
                if (auto subInit = dynamic_cast<Initializer*>((*init)[i]))
                    handleMemcpy(alloc, subInit, node, idx);
                else
                    node->valueMapping[alloc][AllocaHasher{}(alloc, idx)] = (*init)[i];
            }
            else
            {
                handleZeroInit(alloc, node, idx);
            }

            idx.pop_back();
        }
    }
    else
    {
        // 基本类型直接赋值
        node->valueMapping[alloc][AllocaHasher{}(alloc, idx)] = (*init)[0];
    }
}

void GepEvaluator::handleZeroInit(AllocaInst* inst, NodeHandler* node, std::vector<int> index)
{
    // 先定位类型
    Type* ty = inst->GetType(); 
    for (int idx : index)
    {
        if (auto arr = dynamic_cast<ArrayType*>(ty))
        {
            ty = arr->GetSubType();
        }
        else if (auto ptr = dynamic_cast<PointerType*>(ty))
        {
            ty = ptr->GetSubType();
        }
        else
        {
            break; // 到基本类型
        }
    }

    // 针对数组类型递归展开
    if (auto arr = dynamic_cast<ArrayType*>(ty))
    {
        for (int i = 0; i < arr->GetNum(); i++)
        {
            auto subIdx = index;
            subIdx.push_back(i);
            handleZeroInit(inst, node, subIdx);
        }
        return;
    }

    // 基础类型：生成零常量
    Value* zeroVal = nullptr;
    switch (ty->GetTypeEnum())
    {
    case IR_Value_INT:
        zeroVal = ConstIRInt::GetNewConstant(0);
        break;
    case IR_INT_64:
        zeroVal =ConstIRInt::GetNewConstant(0); 
        break;
    case IR_Value_Float:
        zeroVal = ConstIRFloat::GetNewConstant(0.0f);
        break;
    case IR_PTR:
    case BACKEND_PTR:
        zeroVal = ConstPtr::GetNewConstant(nullptr);
        break;
    default:
        zeroVal = nullptr; // void 类型就不处理
        break;
    }

    if (zeroVal)
    {
        AllocaHasher hasher;
        size_t h = hasher(inst, index);
        node->valueMapping[inst][h] = zeroVal;
    }
}

void GepEvaluator::processBlock(ValueMap &addr, NodeHandler *node)
{
    auto &valUseList = node->Block()->GetValUseList();
    if (valUseList.GetSize() < 2)
        return;

    std::vector<ValueMap *> maps;
    for (auto user : valUseList)
    {
        Instruction *inst = static_cast<Instruction *>(user->GetUser());
        BasicBlock *userBlock = inst->GetParent();
        auto it = blockMap.find(userBlock);
        if (it != blockMap.end() && it->second)
        {
            maps.push_back(&(it->second->valueMapping));
        }
        else
        {
            for (auto alloca : allocas)
                alloca->HasStored = true; // 标记所有 allocas
            addr.clear();
            return;
        }
    }

    // 找出所有 map 的共同 key
    std::unordered_set<Value *> commonKeys;
    if (!maps.empty())
    {
        // 从第一个 map 中提取 key
        for (auto &[key, _] : *(maps.front()))
            commonKeys.insert(key);

        for (size_t i = 1; i < maps.size(); ++i)
        {
            std::unordered_set<Value *> temp;
            for (auto &[key, _] : *(maps[i]))
            {
                if (commonKeys.count(key))
                    temp.insert(key);
            }
            commonKeys.swap(temp);
            if (commonKeys.empty())
                break;
        }
    }

    // 删除 addr 中的共同 key
    for (auto key : commonKeys)
    {
        addr.erase(key);
    }
}

bool GepEvaluator::run()
{
    domTree = analysisMgr.get<DominantTree>(function);
    analysisMgr.get<SideEffect>(&Singleton<Module>());

    bool changed = false;
    blockMap.clear();

    std::deque<NodeHandler*> workStack;
    BasicBlock* entry = function->GetFront();
    auto* root = domTree->getNode(entry);

    workStack.emplace_back(new NodeHandler(
        domTree,
        root,
        root->idomChild.begin(),
        root->idomChild.end(),
        ValueMap{}
    ));
    blockMap[entry] = workStack.back();

    while (!workStack.empty())
    {
        NodeHandler* current = workStack.back();
        if (!current->isDone())
        {
            changed |= processNode(current);
            current->markDone();
        }
        else if (current->ChildIter() != current->EndIter())
        {
            auto nextNode = *current->NextChildIter();
            workStack.emplace_back(new NodeHandler(
                domTree,
                nextNode,
                nextNode->idomChild.begin(),
                nextNode->idomChild.end(),
                current->valueMapping
            ));
            blockMap[nextNode->curBlock] = workStack.back();
        }
        else
        {
            workStack.pop_back();
        }
    }

    while (!pendingDelete.empty())
    {
        Instruction* inst = pendingDelete.back();
        pendingDelete.pop_back();
        inst->ClearRelation();
        inst->EraseFromManager();
    }
    return changed;
}

size_t GepHasher::operator()(GepInst* gep, ValueMap* addr) const
{
    if (!gep) return 0;

    Value* base = gep->GetOperand(0);

    // 统一 Value 哈希函数
    auto hashValue = [](Value* val) -> size_t {
        if (!val) return 0;
        if (auto cint = dynamic_cast<ConstIRInt*>(val))
            return ((std::hash<int>()(cint->GetVal() + 3) * 10107 ^
                     std::hash<int>()(cint->GetVal() + 5) * 137) * 157);
        return std::hash<Value*>()(val) ^ (std::hash<Value*>()(val) << 3);
    };

    size_t h = 0;

    // 处理 base
    if (auto alloca = dynamic_cast<AllocaInst*>(base))
        h ^= std::hash<AllocaInst*>()(alloca);
    else if (auto gep_base = dynamic_cast<GepInst*>(base))
        h ^= (*this)(gep_base, addr);
    else
        h ^= hashValue(base);

    // 遍历 gep 的操作数
    int j = 0;
    for (size_t i = 1; i < gep->GetUserUseList().size(); ++i)
    {
        Value* op = gep->GetUserUseList()[i]->usee;
        h += hashValue(op) * ((j + 4) * 107);
        ++j;
    }

    // 累加 addr 映射的哈希
    if (addr)
    {
        auto it = addr->find(base);
        if (it != addr->end())
        {
            for (auto& [idx, val] : it->second)
            {
                h ^= std::hash<size_t>()(idx) + 0x9e3779b9 + (h << 6) + (h >> 2);
                if (val) h ^= hashValue(val) + 0x9e3779b9 + (h << 6) + (h >> 2);
            }
        }
    }

    return h;
}
