#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/Dominant.hpp"
#include "Passbase.hpp"

class ConstantHoist : public _PassBase<ConstantHoist, Function>
{
private:
    struct HoistEntry
    {
        Instruction* lhsInst;
        Value* lhsValue;
        Instruction* rhsInst;
        Value* rhsValue;
        int index;

        HoistEntry(Instruction* lhsI, Value* lhsV, Instruction* rhsI, Value* rhsV, int idx)
            : lhsInst(lhsI), lhsValue(lhsV), rhsInst(rhsI), rhsValue(rhsV), index(idx) {}
    };

    Function* function;
    DominantTree* domTree;
    std::vector<std::unique_ptr<HoistEntry>> hoistEntries;
    std::unordered_map<BasicBlock*, std::unordered_map<Instruction*, int>> instructionIndices;

    // 对单个基本块进行常量提升
    bool processBlock(BasicBlock* bb);

    // 尝试在条件块间提升指令
    bool hoistInstructionsBetweenBlocks(BasicBlock* trueBlock, BasicBlock* falseBlock);

public:
    explicit ConstantHoist(Function* func)
        : function(func), domTree(nullptr) {}

    ~ConstantHoist() = default;

    bool run();
};
