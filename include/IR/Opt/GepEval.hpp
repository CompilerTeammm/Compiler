#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/SideEffect.hpp"
#include "../Analysis/Dominant.hpp"
#include "Passbase.hpp"

class AnalysisManager;
typedef std::unordered_map<Value *, std::unordered_map<size_t, Value *>> ValueAddr_Struct;

struct InitHash
{
    size_t operator()(AllocaInst *alloca, std::vector<int> index)
    {
        std::reverse(index.begin(), index.end());
        size_t hash = 0;
        hash ^= std::hash<AllocaInst *>()(alloca);
        int j = 0;
        for (auto i : index)
        {
            hash += (((std::hash<int>()(i + 3) * 10107 ^ std::hash<int>()(i + 5) * 137) * 157) * ((j + 4) * 107));
            j++;
        }
        return hash;
    }
};

struct ValueHash
{
    size_t operator()(Value *val) const
    {
        if (auto val_int = dynamic_cast<ConstIRInt *>(val))
            return ((std::hash<int>()(val_int->GetVal() + 3) * 10107 ^ std::hash<int>()(val_int->GetVal() + 5) * 137) *
                    157);
        else
            return std::hash<Value *>{}(val) ^ (std::hash<Value *>{}(val) << 3);
    }
};

struct GepHash
{
    size_t operator()(GepInst *gep, ValueAddr_Struct *addr) const
    {
        size_t h = 0;
        Value *Base = gep->GetOperand(0);
        if (!Base->isParam())
        {
            if (auto alloca = dynamic_cast<AllocaInst *>(Base))
            {
                h ^= std::hash<AllocaInst *>()(alloca);
                int j = 0;
                for (int i = 2; i < gep->GetUserUseList().size(); i++)
                {
                    auto p = gep->GetUserUseList()[i]->usee;
                    if (auto load = dynamic_cast<LoadInst *>(p))
                    {
                        if (auto load_gep = dynamic_cast<GepInst *>(load->GetUserUseList()[0]->usee))
                        {
                            size_t load_gep_hash = GepHash{}(load_gep, addr);
                            if (addr->find(load_gep->GetUserUseList()[0]->usee) != addr->end())
                            {
                                if (addr->find(load_gep->GetUserUseList()[0]->usee)->second.find(load_gep_hash) !=
                                    addr->find(load_gep->GetUserUseList()[0]->usee)->second.end())
                                {
                                    h += ValueHash{}(
                                             addr->find(load_gep->GetUserUseList()[0]->usee)->second[load_gep_hash]) *
                                         ((j + 4) * 107);
                                }
                                else
                                {
                                    h += (std::hash<int>{}(load_gep_hash) << (j + 3) * 101);
                                }
                            }
                            else
                            {
                                h += (std::hash<int>{}(load_gep_hash) << (j + 3) * 333);
                            }
                        }
                        else
                        {
                            h += ValueHash{}(p) * ((j + 111) * 123) * (j + 1);
                        }
                    }
                    else
                        h += ValueHash{}(p) * ((j + 4) * 107);
                    j++;
                }
            }
            else if (auto gep_base = dynamic_cast<GepInst *>(Base))
            {
                auto alloca = dynamic_cast<AllocaInst *>(gep_base->GetOperand(0));
                int j = 0;
                for (int i = 1; i < gep->GetUserUseList().size(); i++)
                {
                    auto p = gep->GetUserUseList()[i]->usee;
                    if (auto load = dynamic_cast<LoadInst *>(p))
                    {
                        if (auto load_gep = dynamic_cast<GepInst *>(load->GetUserUseList()[0]->usee))
                        {
                            size_t load_gep_hash = GepHash{}(load_gep, addr);
                            if (addr->find(load_gep->GetUserUseList()[0]->usee) != addr->end())
                            {
                                if (addr->find(load_gep->GetUserUseList()[0]->usee)->second.find(load_gep_hash) !=
                                    addr->find(load_gep->GetUserUseList()[0]->usee)->second.end())
                                {
                                    h += ValueHash{}(
                                             addr->find(load_gep->GetUserUseList()[0]->usee)->second[load_gep_hash]) *
                                         ((j + 4) * 107);
                                }
                                else
                                {
                                    h += (std::hash<int>{}(load_gep_hash) << (j + 3) * 101);
                                }
                            }
                            else
                            {
                                h += (std::hash<int>{}(load_gep_hash) << (j + 3) * 333);
                            }
                        }
                    }
                    else
                        h += ValueHash{}(p) * ((j + 4) * 107);
                    j++;
                }
            }
        }
        else
        {
            if (dynamic_cast<HasSubType *>(Base->GetType())->GetSubType()->GetTypeEnum() == IR_DataType::IR_ARRAY)
            {
                auto array = dynamic_cast<ArrayType *>(dynamic_cast<HasSubType *>(Base->GetType())->GetSubType());
                h ^= (std::hash<Value *>()(Base) << array->GetNum());
            }
            else
                h ^= std::hash<Var *>()(dynamic_cast<Var *>(Base));
            int j = 0;
            for (int i = 1; i < gep->GetUserUseList().size(); i++)
            {
                auto p = gep->GetUserUseList()[i]->usee;
                h ^= ValueHash{}(p) * ((j + 4) * 107);
                j++;
            }
        }
        return h;
    }
};

class GepEvaluate : public _PassBase<GepEvaluate, Function>
{
    class HandleNode
    {
      private:
        BasicBlock *block;
        DominantTree *DomTree;
        DominantTree::TreeNode *dom_node;
        bool Processed = false;
        std::list<DominantTree::TreeNode *>::iterator Curiter;
        std::list<DominantTree::TreeNode *>::iterator Enditer;

      public:
        ValueAddr_Struct ValueAddr;

      public:
        std::list<DominantTree::TreeNode *>::iterator Child() { return Curiter; }
        std::list<DominantTree::TreeNode *>::iterator NextChild() {
            auto iter = Curiter;
            ++Curiter;
            return iter;
        }
        std::list<DominantTree::TreeNode *>::iterator EndIter() { return Enditer; }
        bool isProcessed()
        {
            return Processed;
        }
        void Process()
        {
            Processed = true;
        }
        BasicBlock *GetBlock()
        {
            return block;
        }
    HandleNode(DominantTree *dom,
               DominantTree::TreeNode *node,
               std::list<DominantTree::TreeNode *>::iterator child,
               std::list<DominantTree::TreeNode *>::iterator end,
               std::unordered_map<Value *, std::unordered_map<size_t, Value *>> ValueAddr)
        : DomTree(dom), dom_node(node), Curiter(child), Enditer(end), ValueAddr(ValueAddr) {
        block = node->curBlock; 
    }
    };

  private:
    Function *func;
    DominantTree *DomTree;
    AnalysisManager &AM;
    std::vector<Instruction *> wait_del;//从User改成Instruction了
    std::unordered_map<BasicBlock *, HandleNode *> Mapping;
    std::unordered_map<AllocaInst *, Initializer *> AllocaInitMap;
    std::unordered_set<AllocaInst *> allocas;
    bool ProcessNode(HandleNode *node);
    void HandleMemcpy(AllocaInst *inst, Initializer *init, HandleNode *node, std::vector<int> index);
    void HandleZeroInitializer(AllocaInst *inst, HandleNode *node, std::vector<int> index);
    void HandleBlockIn(ValueAddr_Struct &addr, HandleNode *node);

  public:
    GepEvaluate(Function *_func, AnalysisManager &_AM) : func(_func), AM(_AM)
    {
    }
    bool run();
};