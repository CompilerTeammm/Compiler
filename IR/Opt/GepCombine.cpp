#include "../../include/IR/Opt/GepCombine.hpp"
#include "../../include/IR/Analysis/SideEffect.hpp"
#include "../../include/IR/Opt/ConstantFold.hpp"
#include <algorithm>
#include <deque>

bool GepCombine::run()
{
    DomTree = AM.get<DominantTree>(func);
    AM.get<SideEffect>(&Singleton<Module>());
    bool modified = false;
    std::deque<HandleNode *> WorkList;
    BasicBlock *entryBB = func->GetFront();
    DominantTree::TreeNode *entryNode = DomTree->getNode(entryBB);
    WorkList.push_back(new HandleNode(DomTree,
                                    entryNode,
                                    entryNode->idomChild.begin(),
                                    entryNode->idomChild.end(),
                                    std::unordered_set<GepInst*>()));
    while (!WorkList.empty())
    {
        GepCombine::HandleNode *cur = WorkList.back();
        if (!cur->isProcessed())
        {
            modified |= ProcessNode(cur);
            cur->Process();
            cur->SetChildGeps(cur->GetGeps());
        }
        else if (cur->Child() != cur->EndIter())
        {
            auto childIter = cur->NextChild();
            DominantTree::TreeNode* childNode = *childIter;
            WorkList.push_back(new HandleNode(DomTree,
                                            childNode,
                                            childNode->idomChild.begin(),
                                            childNode->idomChild.end(),
                                            cur->GetChildGeps()));
        }
        else
        {
            delete cur;
            WorkList.pop_back();
        }
    }
    while(!wait_del.empty())
    {
        auto inst = wait_del.back();
        wait_del.pop_back();
        delete inst;
    }
    return modified;
}

bool GepCombine::ProcessNode(HandleNode *node)
{
    bool modified = false;
    BasicBlock *block = node->GetBlock();
    std::unordered_set<GepInst *> geps = node->GetGeps();
    for (Instruction*inst : *block)
    {
        if (auto gep = dynamic_cast<GepInst *>(inst))
        {
            geps.insert(gep);
            if (auto val = SimplifyGepInst(gep, geps))
            {
                modified |= true;
                continue;
            }

            if (GepInst *new_gep = HandleGepPhi(gep, geps))
            {
                gep = new_gep;
                modified |= true;
            }

            if (GepInst *new_gep = Normal_Handle_With_GepBase(gep, geps))
            {
                modified |= true;
            }
        }
    }
    node->SetGeps(geps);
    return modified;
}

Value *GepCombine::SimplifyGepInst(GepInst *inst, std::unordered_set<GepInst *> &geps)
{
    Value *Base = inst->GetUserUseList()[0]->usee;
    if (inst->GetUserUseList().size() == 1)
    {
        inst->ReplaceAllUseWith(Base);
        geps.erase(inst);
        return Base;
    }

    if (dynamic_cast<UndefValue *>(Base))
    {
        inst->ReplaceAllUseWith(UndefValue::Get(inst->GetType()));
        geps.erase(inst);
        return UndefValue::Get(inst->GetType());
    }

    if (auto base_gep = dynamic_cast<GepInst *>(Base))
    {
        auto offset = dynamic_cast<ConstIRInt *>(base_gep->GetUserUseList().back()->usee);
        if (offset && offset->GetVal() == 0)
        {
            std::vector<Operand> Ops;
            for (int i = 1; i < base_gep->GetUserUseList().size() - 1; i++)
                Ops.push_back(base_gep->GetUserUseList()[i]->GetValue());
            for (int i = 1; i < inst->GetUserUseList().size(); i++)
                Ops.push_back(inst->GetUserUseList()[i]->usee);
            GepInst *New_Gep = new GepInst(base_gep->GetUserUseList()[0]->usee, Ops);
            BasicBlock::List<BasicBlock, Instruction>::iterator Inst_Pos(inst);
            Inst_Pos.InsertBefore(New_Gep);
            inst->ReplaceAllUseWith(New_Gep);
            geps.erase(inst);
            geps.insert(New_Gep);
            wait_del.push_back(inst);
            return New_Gep;
        }
    }

    if (inst->GetUserUseList().size() == 2)
    {
        Value *Op1 = inst->GetUserUseList()[1]->usee;
        if (auto val = dynamic_cast<ConstantData *>(Op1))
        {
            if (val->isConstZero())
            {
                inst->ReplaceAllUseWith(Base);
                geps.erase(inst);
                return Base;
            }
        }
        {
            if (auto tp = dynamic_cast<HasSubType *>(Base->GetType()))
            {
                if (tp->GetSize() == 0)
                {
                    inst->ReplaceAllUseWith(Base);
                    geps.erase(inst);
                    return Base;
                }
            }
        }
    }

    return nullptr;
}

GepInst *GepCombine::HandleGepPhi(GepInst *inst, std::unordered_set<GepInst *> &geps)
{
    Value *Base = inst->GetUserUseList()[0]->usee;
    if (auto Phi = dynamic_cast<PhiInst *>(Base))
    {
        auto val = dynamic_cast<GepInst *>(Phi->GetVal(0));
        if (!val)
            return nullptr;
        if (val == inst)
            return nullptr;

        int Not_Same_Pos = -1;
        for (int i = 1; i < Phi->GetUserUseList().size(); i++)
        {
            auto val_ = dynamic_cast<GepInst *>(Phi->GetVal(i));
            if (!val_ || val_->GetUserUseList().size() != val->GetUserUseList().size())
                return nullptr;
            if (val_ == inst)
                return nullptr;

            Type *CurTp = nullptr;

            for (int j = 0; j < val->GetUserUseList().size(); j++)
            {
                if (val->GetOperand(j)->GetType() != val_->GetOperand(j)->GetType())
                    return nullptr;
                Value *Val_j = val->GetOperand(j);
                Value *Val_j_ = val_->GetOperand(j);
                auto const_Val_j = dynamic_cast<ConstantData *>(Val_j);
                auto const_Val_j_ = dynamic_cast<ConstantData *>(Val_j_);
                if (const_Val_j && const_Val_j_)
                {
                    if (auto const_Val_j_int = dynamic_cast<ConstIRInt *>(const_Val_j))
                    {
                        if (auto const_val_j_int_ = dynamic_cast<ConstIRInt *>(const_Val_j_))
                        {
                            if (const_Val_j_int->GetVal() != const_val_j_int_->GetVal())
                                return nullptr;
                        }
                        else if (auto const_val_j_float_ = dynamic_cast<ConstIRFloat *>(const_Val_j_))
                        {
                            if (const_Val_j_int->GetVal() != const_val_j_float_->GetVal())
                                return nullptr;
                        }
                    }
                    else if (auto const_Val_j_float = dynamic_cast<ConstIRFloat *>(const_Val_j))
                    {
                        if (auto const_val_j_int_ = dynamic_cast<ConstIRInt *>(const_Val_j_))
                        {
                            if (const_Val_j_float->GetVal() != const_val_j_int_->GetVal())
                                return nullptr;
                        }
                        else if (auto const_val_j_float_ = dynamic_cast<ConstIRFloat *>(const_Val_j_))
                        {
                            if (const_Val_j_float->GetVal() != const_val_j_float_->GetVal())
                                return nullptr;
                        }
                    }
                }
                else if (Val_j != Val_j_)
                {
                    if (Not_Same_Pos == -1)
                        Not_Same_Pos = j;
                    else
                        return nullptr;
                }
            }
        }
        if (Phi->GetValUseList().GetSize() != 1)
            return nullptr;
        std::vector<Operand> Ops;
        for (int i = 1; i < val->GetUserUseList().size(); i++)
            Ops.push_back(val->GetOperand(i));
        GepInst *New_Gep = new GepInst(val->GetUserUseList()[0]->usee, Ops);
        if (Not_Same_Pos == -1)
        {
            BasicBlock::List<BasicBlock, Instruction>::iterator Inst_Pos(inst);
            Inst_Pos.InsertBefore(New_Gep);
        }
        else
        {
            PhiInst *NewPhi = new PhiInst(Phi, Phi->GetType());
            for (auto &[key, val] : Phi->PhiRecord)
                NewPhi->addIncoming(val.first, val.second);
            New_Gep->ReplaceSomeUseWith(Not_Same_Pos, NewPhi);
            BasicBlock::List<BasicBlock, Instruction>::iterator Inst_Pos(inst);
            Inst_Pos.InsertBefore(New_Gep);
            New_Gep->ReplaceSomeUseWith(Not_Same_Pos, NewPhi);
        }
        inst->ReplaceSomeUseWith(0, New_Gep);
        Base = New_Gep;
        return inst;
    }
    return nullptr;
}

GepInst *GepCombine::Normal_Handle_With_GepBase(GepInst *inst,
                                                          std::unordered_set<GepInst *> &geps)
{
    Value *Base = inst->GetUserUseList()[0]->usee;
    if (auto base = dynamic_cast<GepInst *>(Base))
    {
        if (!CanHandle(inst, base))
            return nullptr;
        if (auto base_base = dynamic_cast<GepInst *>(base->GetUserUseList()[0]->usee))
            if (base_base->GetUserUseList().size() == 2 && CanHandle(base, base_base))
                return nullptr;
    }
    {
        if (inst->GetUserUseList().size() == 3)
        {
            if (dynamic_cast<ConstantData *>(inst->GetUserUseList()[1]->usee) &&
                dynamic_cast<ConstantData *>(inst->GetUserUseList()[1]->usee)->isConstZero())
            {
                Value *val = inst->GetUserUseList()[2]->usee;
                if (auto binary = dynamic_cast<BinaryInst *>(val))
                {
                    if (binary->GetOp() == BinaryInst::Operation::Op_Add)
                    {
                        if (auto constdata = dynamic_cast<ConstantData *>(binary->GetUserUseList()[1]->usee))
                        {
                            if (auto gep = dynamic_cast<GepInst *>(binary->GetUserUseList()[0]->usee))
                            {
                                if (gep->GetUserUseList()[0]->usee == Base && geps.count(gep))
                                {
                                    inst->ReplaceSomeUseWith(inst->GetUserUseList()[0].get(), gep);
                                    inst->ReplaceSomeUseWith(inst->GetUserUseList()[1].get(), constdata);
                                }
                            }
                        }
                        else if (auto constdata = dynamic_cast<ConstantData *>(binary->GetUserUseList()[0]->usee))
                        {
                            if (auto gep = dynamic_cast<GepInst *>(binary->GetUserUseList()[1]->usee))
                            {
                                if (gep->GetUserUseList()[0]->usee == Base && geps.count(gep))
                                {
                                    inst->ReplaceSomeUseWith(inst->GetUserUseList()[0].get(), gep);
                                    inst->ReplaceSomeUseWith(inst->GetUserUseList()[1].get(), constdata);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

bool GepCombine::CanHandle(GepInst *src, GepInst *base)
{
    auto inst_all_offset_zero = [&src]() {
        return std::all_of(src->GetUserUseList().begin() + 1, src->GetUserUseList().end(), [](User::UsePtr &use) {
            if (auto val = dynamic_cast<ConstantData *>(use->usee))
            {
                return val->isConstZero();
            }
            return false;
        });
    };
    auto base_all_offset_zero = [&base]() {
        return std::all_of(base->GetUserUseList().begin() + 1, base->GetUserUseList().end(), [](User::UsePtr &use) {
            if (auto val = dynamic_cast<ConstantData *>(use->usee))
            {
                return val->isConstZero();
            }
            return false;
        });
    };
    if (inst_all_offset_zero() && !base_all_offset_zero() && base->GetValUseList().GetSize() != 1)
        return false;
    return true;
}