#include "../../include/IR/Opt/GVN.hpp"
#include "../../include/lib/CoreClass.hpp"
#include <functional>
#include <utility>

// hash_table  old_gvn global 

// as for globalInsts
// 学习cmmc，但是有一些东西需要修改
// 1. 对语句的判断，并不是所有的语句都可以使用 gnv 优化消除
// 2. 对支配关系的错判，修改
struct GlobalInstHasher 
{
    std::unordered_map<const Instruction*,size_t>& cachedHash;
    std::function<uint32_t(Value*)>& getNumber;

    size_t operator()(Instruction* inst) const {
        // 存在直接返回
        const auto iter = cachedHash.find(inst);
        if(iter != cachedHash.cend())
            return iter->second;
        // 使用哈希模板函数生成哈希值
        size_t hashVal = std::hash<Instruction::Op>{} (inst->GetInstId());
        for(int i = 0; i < inst->GetOperandNums(); i++)
        {
            hashVal = hashVal*131 + std::hash<uint32_t>{} (getNumber(inst->GetOperand(i)));
        }

        hashVal = std::hash<size_t>{} (hashVal);
        cachedHash.emplace(inst,hashVal);
        return hashVal;
    }
};


// compare the two ints hasher
struct GlobalInstEqual 
{
    std::function<uint32_t(Value*)>& getNumber;
    bool operator()(Instruction* lhs,Instruction* rhs) const
    {
        if(lhs->GetOperandNums() != rhs->GetOperandNums())
            return false;
        if(lhs->GetInstId() != rhs->GetInstId())
            return false;
        std::vector<Value*> rhsOps;
        for(int i = 0; i < rhs->GetOperandNums(); i++)
        {
            rhsOps.push_back(lhs->GetOperand(i));
        }
        std::vector<Value*> lhsOps;
        for (int i = 0; i < lhs->GetOperandNums(); i++)
        {
            lhsOps.push_back(lhs->GetOperand(i));
        }
        return std::equal(lhsOps.begin(), lhsOps.end(), rhsOps.begin(),
                          [&](Value* lhsVal, Value* rhsVal)
                          { 
                                 return getNumber(lhsVal) == getNumber(rhsVal);
                          });
    }
};

bool GVN::run()
{
    bool hasChange = false;
    uint32_t numbingID = 0;

    // 存储值和编号
    // unordered_map 实现是哈希，key必须可以提供哈希值
    std::unordered_map<Value*,uint32_t> valueNumber;

    const auto getValueFromVN = [&](Value* value)
    {
        const auto iter = valueNumber.find(value);
        // 存在即返回
        if(iter!=valueNumber.cend())
            return iter->second;
        //不存在返回新值
        const auto id = numbingID++;
        valueNumber.emplace(std::make_pair(value, id));

        return id;
    };

    //函数包装器，将​​任何可调用对象​
    //​（如普通函数、Lambda表达式、函数对象等）封装成统一类型
    std::function<uint32_t(Value*)> getNumber;
    
    // 存储Inst指令，与编号
    std::unordered_map<const Instruction*,size_t> cachedHash;

    GlobalInstHasher InstHasher{cachedHash,getNumber};
    GlobalInstEqual InstEqual {getNumber};

    std::unordered_map<size_t, 
                      std::vector<
                      std::pair<uint32_t, std::unordered_set<Instruction*>>
                      >> instNumber;
        
    
    //求取Inst 的哈希值
    const auto getInstNumber = [&](Instruction* inst)
    {
        const auto hash = InstHasher(inst);
        auto& val = instNumber[hash];

        for(auto& [id,rhs]:val){
            if(rhs.count(inst))
                return id;
            if(InstEqual(inst,*rhs.begin())){
                rhs.insert(inst);
                return id;
            }
        }
        const auto id = numbingID++;
        val.emplace_back(id,std::unordered_set<Instruction*> {inst});
        return id;
    };

    // need to fixed
    getNumber = [&](Value* value) 
    {
        return getValueFromVN(value);
    };

    // uint32_t 是计算出来的hashVal值
    std::unordered_map<uint32_t, std::vector<Instruction *>> instructions;

    //对全部的值进行了值编号
    for (auto block : func->GetBBs())
    {
        for (auto inst : *block)
        {
            const auto id = getInstNumber(inst);
            instructions[id].push_back(inst);
        }
    }

    std::vector<Value *> operandMap(numbingID);
    for (auto [key, val] : valueNumber)
        operandMap[val] = key;

    for (uint32_t id = 0; id < numbingID; ++id)
    {
        const auto iter = instructions.find(id);
        if (iter == instructions.cend())
            continue;

        auto &sameInstructions = iter->second;

        if (sameInstructions.size() == 1)
        {
            operandMap[id] = sameInstructions.front();
            continue;
        }

        BasicBlock *block = nullptr;
        for (auto inst : sameInstructions)
        {
            if (block == nullptr)
                block = inst->GetParent();
            else
            {
                if(!tree->dominates(block,inst->GetParent()))
                    block = inst->GetParent();
            }
        }

        Instruction *replaceInst = nullptr;
        for (auto inst : sameInstructions)
        {
            if (block == inst->GetParent())
            {
                replaceInst = inst;
                break;
            }
        }

        for (auto inst : sameInstructions)
        {
            if (replaceInst != inst)
            {
                inst->ReplaceAllUseWith(replaceInst);
                delete inst;
                hasChange = true;
            }
        }
    }

    return hasChange;
}






// 暴力实现思路，对每一条指令进行一个属性求知
// 记录sameInstructions，进行消除
// bool GVN:: run()
// {
//     bool hasChange = false;
//     int valNum = 0;
//     // auto it = func->begin();
//     // std::vector<std::pair<Instruction*,>> sameInsts;
//     // std::pair<Value*,Value*> ops;

//     using Ops = std::pair<Value*,Value*>;
//     using property =std::pair<Instruction::Op,Ops>; 
//     // std::unordered_map<property,Instruction*> VNInsts;
//     std::map<property,Instruction*> VNInsts;
//     std::vector<std::pair<Instruction*,property>> sameInsts;

//     for(auto BB : *func)
//     {
//         for(auto I : *BB)
//         {
//             if (I->IsBinary())
//             {
//                 Value* op1  = I->GetOperand(0);
//                 Value* op2 = I->GetOperand(1);
//                 auto property = std::make_pair(I->GetInstId(),
//                                 std::make_pair(op1,op2));
//                 if(VNInsts.find(property) == VNInsts.end())
//                     VNInsts.emplace(std::make_pair(property, I));
//                 else {
//                     sameInsts.emplace_back(std::make_pair(I,property));
//                 }
//             }
//         }
//     }

//     for(auto [val,proper] : sameInsts)
//     {
//         Value* repalce = VNInsts[proper];
//         val->ReplaceAllUseWith(repalce);
//         delete val;
//     }

//     return hasChange;
// }
