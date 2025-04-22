#include "../../include/IR/Opt/GVN.hpp"

// 暴力实现思路，对每一个操作数进行一个编号
// 记录sameInstructions，进行消除
bool GVN:: run()
{
    bool hasChange = false;
    int valNum = 0;
    // auto it = func->begin();
    std::vector<Instruction*> sameInsts;
    for(auto BB : *func)
    {
        for(auto I : *BB)
        {
            int flag = 0;
            for(int i = 0; i < I->GetOperandNums(); i++)
            {
                Value* op = I->GetOperand(i);
                if(ValTable.find(op) == ValTable.end()){
                    ValTable[op] = valNum++;
                    flag = 1;
                }
            }
            
            if(flag == 0)
                sameInsts.push_back(I);
        }
    }

    for(auto I : sameInsts)
        delete I;
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

