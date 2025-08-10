#include "../../include/IR/Analysis/SideEffect.hpp"

bool SideEffect::RunOnModule(Module *module){
    for(auto &func:module->GetFuncTion()){
        func->Change_Val.clear();
        CreateCallMap(func.get());
    }
    DetectRecursive();
    for (auto& func : module->GetFuncTion()) {
        AnalyzeFuncSideEffect(func.get());
    }
    return false;
}

void SideEffect::CreateCallMap(Function *func){
    for(BasicBlock* bb:*func){
        for (Instruction* inst : *bb) {
            if(auto call=dynamic_cast<CallInst*>(inst)){
                auto target=dynamic_cast<Function*>(call->GetUserUseList()[0]->usee);
                if(target){
                    CallingFuncs[func].insert(target);
                    CalleeFuncs[target].insert(func);
                }
            }
        }
    }
}

void SideEffect::DetectRecursive() {
    std::set<Function*> visited;
    for (auto& func : module->GetFuncTion()) {
        VisitFunc(func.get(), visited);
    }
}
//记得最后都看看哪里需要补判空
void SideEffect::VisitFunc(Function* entry, std::set<Function*>& visited){
    visited.insert(entry);
    for (Function* callee : CallingFuncs[entry]){
        if (visited.count(callee)) {
            RecursiveFuncs.insert(callee);
        } else {
            VisitFunc(callee, visited);
        }
    }
    visited.erase(entry);
}

void SideEffect::AnalyzeFuncSideEffect(Function* func){
    auto& params=func->GetParams();
    for (auto& param : params){
        if(param->GetTypeEnum()==IR_PTR){
            for (Use* u : param->GetValUseList()){
                if(auto store = dynamic_cast<StoreInst*>(u->GetUser())){
                    func->Change_Val.insert(param.get());
                }
                for(Use* u2 : u->GetUser()->GetValUseList()){
                    if (auto store2 = dynamic_cast<StoreInst*>(u2->GetUser())){
                        func->Change_Val.insert(param.get());
                    }
                }
            }
        }
    }
    if (FuncHasSideEffect(func)) {
        func->HasSideEffect = true;
    }
}

bool SideEffect::FuncHasSideEffect(Function* func){
    // 若设置开启递归分析，且该函数是递归函数，则认为有副作用
    if (Judge && RecursiveFuncs.count(func)){
        return true;
    }
    // 并行函数或循环展开体默认认为有副作用
    if(func->tag==Function::ParallelBody || func->tag == Function::UnrollBody){
        return true;
    }

    for (BasicBlock* bb : *func){
        for (Instruction* inst : *bb){
            if (auto call = dynamic_cast<CallInst*>(inst)){
                auto& operandList = call->GetUserUseList();
                if (operandList.empty() || !operandList[0] || !operandList[0]->usee) continue;

                Function* target = dynamic_cast<Function*>(operandList[0]->usee);
                std::string name = target ? target->GetName() : "";

                if (name == "getint" || name == "getch" || name == "getfloat" || name == "getfarray" ||
                    name == "putint" || name == "putfloat" || name == "putarray" || name == "putfarray" ||
                    name == "putf" || name == "getarray" || name == "putch" || name == "_sysy_starttime" ||
                    name == "_sysy_stoptime" || name == "llvm.memcpy.p0.p0.i32") {
                    return true;
                }

                if (target && target->HasSideEffect){
                    return true;
                }

                if (target){
                    // 检查 Change_Val 参数是否被影响
                    const auto& targetParams = target->GetParams();
                    for (size_t i = 1; i < operandList.size(); ++i) {
                        if (i - 1 >= targetParams.size() || !operandList[i] || !operandList[i]->usee) continue;
                        Value* arg = operandList[i]->usee;
                        Value* param = targetParams[i - 1].get();

                        if (target->Change_Val.count(param)) {
                            func->Change_Val.insert(arg);
                            return true;
                        }
                    }
                }
            }
            else if (auto store = dynamic_cast<StoreInst*>(inst)) {
                if (store->GetOperandNums() < 2) continue;

                Value* dst = store->GetOperand(1);
                if (!dst) continue;

                if (dst->isGlobal()) return true;

                if (auto gep = dynamic_cast<GepInst*>(dst)) {
                    Value* base = gep->GetOperand(0);
                    if (base && base->isGlobal()) {
                        return true;
                    }
                }
            }
            else if(auto binary = dynamic_cast<BinaryInst*>(inst)){
                if(binary->IsAtomic()){
                    for (auto& u : binary->GetUserUseList()){
                        if (u && u->usee) {
                            func->Change_Val.insert(u->usee);
                        }
                    }
                    return true;
                }
            }
        }
    }
    return false;
}