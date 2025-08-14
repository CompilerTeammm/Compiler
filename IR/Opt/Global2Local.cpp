#include "../../include/IR/Opt/Global2Local.hpp"

bool Global2Local::run()
{
    init(module);
    RunPass(module);
    return false;
}

void Global2Local::init(Module* module)
{
    createSuccFuncs(module);
    CreateCallNum(module);
    DetectRecursive(module);
    CalGlobal2Funcs(module);
}  

void Global2Local::createSuccFuncs(Module* module)
{
    for(auto& func_ : module->GetFuncTion())
    {
        Function* func = func_.get();
        for (Use* user : func->GetValUseList())
        {
            if(auto inst = dynamic_cast<CallInst*>(user->GetUser()))
                SuccFuncs[func].insert(inst->GetParent()->GetParent());
        }
    }
}

void Global2Local::DetectRecursive(Module* module)
{
    std::unordered_map<Function*, std::unordered_set<Function*>> CG;
    CG.reserve(module->GetFuncTion().size());

    for (auto& f_ : module->GetFuncTion()) CG[f_.get()];

    for (auto& f_ : module->GetFuncTion()) {
        Function* callee = f_.get();
        for (Use* useSite : callee->GetValUseList()) {
            auto call = dynamic_cast<CallInst*>(useSite->GetUser());
            if (!call) continue;
            BasicBlock* bb = call->GetParent();
            if (!bb) continue;
            Function* caller = bb->GetParent();
            if (!caller) continue;
            CG[caller].insert(callee);
        }
    }

    std::unordered_set<Function*> visited;
    std::unordered_set<Function*> onstack;
    std::vector<Function*> stack; stack.reserve(128);

    std::function<void(Function*)> dfs = [&](Function* f) {
        visited.insert(f);
        onstack.insert(f);
        stack.push_back(f);

        for (Function* g : CG[f]) {
            if (!visited.count(g)) dfs(g);
            else if (onstack.count(g)) {
                for (int i = (int)stack.size()-1; i >= 0; --i) {
                    RecursiveFunctions.insert(stack[i]);
                    if (stack[i] == g) break;
                }
            }
        }

        stack.pop_back();
        onstack.erase(f);
    };

    for (auto& kv : CG) if (!visited.count(kv.first)) dfs(kv.first);
}

void Global2Local::CalGlobal2Funcs(Module* module)
{
    for(auto & global_ptr : module->GetGlobalVariable())
    {
        Var* global = global_ptr.get();
        for(Use* user_ : global->GetValUseList())
        {
            if(User* inst = dynamic_cast<User*>(user_->GetUser())){
                auto inst_=dynamic_cast<Instruction*>(inst);
                Global2Funcs[global].insert(inst_->GetParent()->GetParent());
            }
        }
    }
}

void Global2Local::RunPass(Module* module)
{
    auto& globalvals = module->GetGlobalVariable();
    bool Repeat = false;

    for(auto iter = globalvals.begin(); iter != globalvals.end(); )
    {
        Var* global = iter->get();
        if(!Repeat && global)
        {
            auto Tp = dynamic_cast<HasSubType*>(global->GetType());
            if(Tp->GetSubType()->GetTypeEnum() == IR_DataType::IR_ARRAY)
            {
                ++iter;
            }
            else
            {
                if(Global2Funcs[global].size() == 0)
                {
                    iter = globalvals.erase(iter); // 删除未使用全局变量
                }
                else if(global->usage == Var::Constant || global->ForParallel)
                {
                    iter++;
                }
                else if(Global2Funcs[global].size() == 1)
                {
                    Function* func = *Global2Funcs[global].begin();
                    if(CanLocal(global, func))
                    {
                        LocalGlobalVariable(global, func); //这里一定要调用
                        iter = globalvals.erase(iter);
                    }
                    else
                        iter++;
                }
                else
                    iter++;
            }

            if(iter == globalvals.end())
            {
                Repeat = true;
                iter = globalvals.begin();
            }
        }
        else if(iter == globalvals.end())
            return;
        else if(Repeat && global)
        {
            if(global->usage == Var::Constant || global->ForParallel)
                iter++;
            else if(Global2Funcs[global].size() == 1)
            {
                Function* func = *Global2Funcs[global].begin();
                if(CanLocal(global, func))
                {
                    LocalGlobalVariable(global, func); //这里也必须调用
                    iter = globalvals.erase(iter);
                }
                else
                    iter++;
            }
            else
                iter++;
        }
    }
}


void Global2Local::LocalGlobalVariable(Var* val, Function* func)
{
    auto tp = dynamic_cast<HasSubType*>(val->GetType());
    AllocaInst* alloca_ = new AllocaInst(tp->GetSubType());
    BasicBlock* begin = func->front;
    auto alloca=dynamic_cast<Instruction*>(alloca_);
    alloca->SetManager(begin);
    begin->push_front(alloca);

    if(!val->GetInitializer())
    {
        if(tp->GetSubType()->GetTypeEnum() != IR_DataType::IR_ARRAY)
        {
            auto iter = begin->begin();
            for (; iter != begin->end(); ++iter) 
            {
                if (!dynamic_cast<AllocaInst*>(*iter))
                    break;
            }
            StoreInst* store = new StoreInst(ConstIRInt::GetNewConstant(0), alloca_);
            iter.InsertBefore(store);
            val->ReplaceAllUseWith(alloca_);
        }
        else
        {
            val->ReplaceAllUseWith(alloca_);
            LocalArray(val, alloca_, begin);
        }
    }
    else
    {
        if(tp->GetSubType()->GetTypeEnum() == IR_DataType::IR_ARRAY)
        {
            val->ReplaceAllUseWith(alloca_);
            LocalArray(val, alloca_, begin);
        }
        else
        {
            auto iter = begin->begin();
            for (; iter != begin->end(); ++iter) 
            {
                if (!dynamic_cast<AllocaInst*>(*iter))
                    break;
            }
            Operand init = val->GetInitializer();
            StoreInst* store = new StoreInst(init, alloca_);
            iter.InsertBefore(store);
            val->ReplaceAllUseWith(alloca_);
        }
    }
}

void Global2Local::LocalArray(Var* arr, AllocaInst* alloca, BasicBlock* block)
{
    Operand initializer = arr->GetInitializer();
    Type* tp = arr->GetType();
    std::vector<Operand> args;
    arr->usage = Var::Constant;
    args.push_back(alloca);
    args.push_back(arr);
    if(auto subtp = dynamic_cast<HasSubType*>(tp)->GetSubType())
        args.push_back(ConstIRInt::GetNewConstant(subtp->GetSize()));
    else
        args.push_back(ConstIRInt::GetNewConstant(tp->GetSize()));
    args.push_back(ConstIRBoolean::GetNewConstant(false));
    User* inst = Trival::GenerateCallInst("llvm.memcpy.p0.p0.i32",args);
    auto iter = block->begin();
    for (; iter != block->end(); ++iter) 
    {
        if (!dynamic_cast<AllocaInst*>(*iter))
            break;
    }
    auto inst_=dynamic_cast<Instruction*>(inst);
    iter.InsertBefore(inst_);
}

bool Global2Local::CanLocal(Var* val, Function* func)
{
    auto Sub_Tp = dynamic_cast<HasSubType*>(val->GetType());
    if(Sub_Tp->GetSubType()->GetTypeEnum() == IR_DataType::IR_ARRAY)
    {
        size_t size = Sub_Tp->GetSubType()->GetSize();
        CurrSize += size;
        if(CurrSize > MaxSize)
            return false;
    }

    if(RecursiveFunctions.find(func) != RecursiveFunctions.end())
        return false;

    if(Sub_Tp->GetSubType()->GetTypeEnum() != IR_DataType::IR_ARRAY)
    {
        bool changed = false;
        for(Use* use_ : val->GetValUseList())
        {
            if(StoreInst* inst = dynamic_cast<StoreInst*>(use_->GetUser()))
            {
                changed = true;
                break;
            }
            else if(GepInst* inst = dynamic_cast<GepInst*>(use_->GetUser()))
            {
                for(Use* use__ : inst->GetValUseList())
                    if(dynamic_cast<StoreInst*>(use__->GetUser()))
                    {
                        changed = true;
                        break;
                    }
                if(changed) break;
            }
        }

        if(changed)
        {
            if(CallTimes[func] > 1)
                return false;
            return true;
        }
        else
            return true;
    }
    else
    {
        if(!val->GetInitializer())
        {
            if(auto gep = val->GetValUseList().back())
            {
                if(auto inst = dynamic_cast<GepInst*>(gep->GetUser()))
                {
                    if(auto call = dynamic_cast<CallInst*>(inst->GetValUseList().back()->GetUser()))
                    {
                        if(!(call->GetUserUseList()[0]->usee->GetName() == "getfarray" ||
                             call->GetUserUseList()[0]->usee->GetName() == "getarray"))
                            return false;
                    }
                    else if(!dynamic_cast<StoreInst*>(inst->GetValUseList().back()->GetUser()))
                        return false;
                }
            }
        }

        for(Use* use_ : val->GetValUseList())
        {
            if(GepInst* inst = dynamic_cast<GepInst*>(use_->GetUser()))
            {
                for(Use* use__ : inst->GetValUseList())
                    if(CallInst* call = dynamic_cast<CallInst*>(use__->GetUser()))
                    {
                        int index = call->GetUseIndex(use__);
                        Function* calleefunc = dynamic_cast<Function*>(call->GetUserUseList()[0]->usee);
                        if(calleefunc)
                        {
                            if(hasChanged(index - 1, calleefunc))
                                return false;
                            else
                                return true;
                        }
                    }
            }
        }
    }

    return true;
}

bool Global2Local::hasChanged(int index, Function* func)
{
    Value* val = func->GetParams()[index].get();
    for(Use* use_ : val->GetValUseList())
    {
        if(StoreInst* inst = dynamic_cast<StoreInst*>(use_->GetUser()))
            return true;
        else if(GepInst* inst = dynamic_cast<GepInst*>(use_->GetUser()))
        {
            for(Use* use__ : inst->GetValUseList())
            {
                if(StoreInst* inst_ = dynamic_cast<StoreInst*>(use__->GetUser()))
                    return true;
                else if(CallInst* call = dynamic_cast<CallInst*>(use__->GetUser()))
                {
                    int index_ = call->GetUseIndex(use__);
                    Function* calleefunc = dynamic_cast<Function*>(call->GetUserUseList()[0]->usee);
                    if(calleefunc)
                        return hasChanged(index_ - 1, calleefunc);
                }
            }
        }
        else if(CallInst* inst = dynamic_cast<CallInst*>(use_->GetUser()))
        {
            int index_ = inst->GetUseIndex(use_);
            Function* calleefunc = dynamic_cast<Function*>(inst->GetUserUseList()[0]->usee);
            if(calleefunc)
                return hasChanged(index_ - 1, calleefunc);
        }
        else
            return false;
    }
    return false;
}


void Global2Local::CreateCallNum(Module* module)
{
    CallTimes.clear();

    for (auto& func_ : module->GetFuncTion())
    {
        Function* callee = func_.get();
        // 遍历所有使用 callee 的地方
        for (Use* useSite : callee->GetValUseList())
        {
            User* u = useSite->GetUser();
            auto call = dynamic_cast<CallInst*>(u);
            if (!call) continue;

            auto callInst = dynamic_cast<Instruction*>(u);
            if (!callInst) continue;

            CallTimes[callee]++; // 只统计调用次数，不再判断循环
        }
    }
}