#pragma once
#include "Passbase.hpp"
#include "../Analysis/Dominant.hpp"
#include "../../lib/CoreClass.hpp"
#include "../../lib/CFG.hpp"
#include "../../IR/Analysis/IDF.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
//用于标识唯一表达式,前面的字符串还是会有点问题
struct ExprKey{
    Instruction::Op op;
    Operand lhs;
    Operand rhs;
    bool operator==(const ExprKey& other)const{
        return op==other.op&&lhs==other.lhs&&rhs==other.rhs;
    }
    struct Hash{
        size_t operator()(const ExprKey& k)const{
            return std::hash<Operand>()(k.lhs)^(std::hash<Operand>()(k.rhs)<<1)^(std::hash<int>()(static_cast<int>(k.op))<<2);
        }
    };
};
class SSAPRE :public _PassBase<SSAPRE,Function>
{
    // using TreeNode = DominantTree::TreeNode;
private:
    // using ExprKey = std::string;
    Function* func;
    DominantTree* tree;
    std::unordered_map<ExprKey, std::vector<Instruction*>,ExprKey::Hash> exprToOccurList;
public:
    bool run() override ;
    SSAPRE(Function* _func,DominantTree* _tree): tree(_tree),func(_func){}
    ~SSAPRE() = default;
    bool PartialRedundancyElimination(Function* func);
    //实际上只是检测是否有部分冗余，接下来交给BeginToChange
    // bool BeginToChange();
    //在IDF分析中计算表达式插入点
    std::set<BasicBlock*> ComputeInsertPoints(DominantTree* ,std::set<BasicBlock*>&);
    //在基本块中查找匹配表达式
    Instruction* FindExpressionInBlock(BasicBlock*, const ExprKey&);

};

