#include "../../include/IR/Opt/GVN.hpp"
#include "../../include/lib/CoreClass.hpp"
#include <utility>

// 暴力实现思路，对每一个操作数进行一个编号
// 记录sameInstructions，进行消除
bool GVN:: run()
{
    bool hasChange = false;
    int valNum = 0;
    // auto it = func->begin();
    // std::vector<std::pair<Instruction*,>> sameInsts;
    // std::pair<Value*,Value*> ops;

    using Ops = std::pair<Value*,Value*>;
    using property =std::pair<Instruction::Op,Ops>; 
    // std::unordered_map<property,Instruction*> VNInsts;
    std::map<property,Instruction*> VNInsts;
    std::vector<std::pair<Instruction*,property>> sameInsts;

    for(auto BB : *func)
    {
        for(auto I : *BB)
        {
            if (I->IsBinary())
            {
                Value* op1  = I->GetOperand(0);
                Value* op2 = I->GetOperand(1);
                auto property = std::make_pair(I->GetInstId(),
                                std::make_pair(op1,op2));
                if(VNInsts.find(property) == VNInsts.end())
                    VNInsts.emplace(std::make_pair(property, I));
                else {
                    sameInsts.emplace_back(std::make_pair(I,property));
                }
            }
        }
    }

    for(auto [val,proper] : sameInsts)
    {
        Value* repalce = VNInsts[proper];
        val->ReplaceAllUseWith(repalce);
        delete val;
    }
    
    return hasChange;
}



// 抄的cmmc的
// struct GlobalInstHasher 
// {
//     std::unordered_map<const Instruction*,int>& cachedHash;
//     std::function<int(Value*)> & getNumber;
//     int operator()(const Instruction* inst) const {
//         if(const auto iter = cachedHash.find(inst); iter!=cachedHash.cend())
//             return iter->second;
//         int hashValue = std::hash<Instruction::Op> {}(inst->GetInstId());
        

//         return hashValue;
//     }
// };

// struct GlobalInstEqul
// {   
//     std::function<int(Value*)>& getNumber;
//     bool operator()(const Instruction* lhs, const Instruction* rhs) const 
//     {

//     }
// };

// bool GVN::run() 
// {
//     auto DT = tree;
//     bool modfied = false;
    
//     int allocateID = 0;
//     std::unordered_map<Value*,int> valueNumber;

//     const auto getValueNumber = [&](Value* value)
//     {
//         const auto iter = valueNumber.find(value);
//         if(iter != valueNumber.cend())
//             return iter->second;
//         const auto id = allocateID++;
//         valueNumber.emplace(value,id);
//         return id;
//     };

//     std::function<int(Value*)> getNumber;
//     std::unordered_map<const Instruction*,int> cachedHash;

//     GlobalInstHasher hasher{cachedHash,getNumber};
//     GlobalInstEqul equal {getNumber};
//     std::unordered_map<size_t, std::vector<std::pair<uint32_t, std::unordered_set<Instruction*>>>> instNumber;
    
    
// }

