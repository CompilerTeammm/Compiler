#pragma once
#include "../../lib/CFG.hpp"
#include "../Analysis/SideEffect.hpp"
#include "../Analysis/Dominant.hpp"
#include "Passbase.hpp"
#include <vector>
#include <deque>
#include <unordered_set>
#include <cassert>
#include <iostream>
#include <limits>
#include <sstream>
#include <cstdlib>

static inline bool kGepDebug() {
    const char* p = std::getenv("GEPDEBUG");
    return p && *p && std::string(p) != "0";
}
#define DBG(x) do { if (kGepDebug()) { std::cerr << "[GEPFLATTEN] " << x << "\n"; } } while(0)

class GepFlatten : public _PassBase<GepFlatten, Function> {
private:
    Function *func;

    // 递归收集数组各维度大小，比如 int A[3][4][5] => dims = {3,4,5}
    static void collectArrayDims(Type *t, std::vector<int> &dims) {
        if (!t) return;
        if (t->GetTypeEnum() == IR_DataType::IR_ARRAY) {
            auto arr = dynamic_cast<ArrayType *>(t);
            if (!arr) return;
            int n = arr->GetNum();
            if (n <= 0) {
                DBG("collectArrayDims: skip non-positive dim=" << n);
            } else {
                dims.push_back(n);
            }
            if (auto has = dynamic_cast<HasSubType *>(t))
                collectArrayDims(has->GetSubType(), dims);
        }
    }

    // 创建整型常量
    static ConstIRInt *createConstInt(int v) {
        return ConstIRInt::GetNewConstant(v);
    }

    // 创建GEP指令（仅用现有构造器创建）
    static GepInst *createRawGep(Value *base, const std::vector<Value *> &indices) {
        std::vector<Operand> args;
        args.reserve(indices.size());
        for (auto *val : indices)
            args.push_back((Operand)val);
        return new GepInst((Operand)base, args);
    }

    // 把 BitCast 剥离，返回最底层非 BitCast 的 Value*
    static Value* stripBitCasts(Value* v) {
        while (v) {
            if (auto bc = dynamic_cast<BitCastInst*>(v)) {
                // 假设 Instruction::GetOperand(0) 返回源操作数
                if (bc->GetOperandNums() > 0) {
                    Value* src = bc->GetOperand(0);
                    if (src == v) break; // 防御自引用
                    v = src;
                    continue;
                } else break;
            }
            break;
        }
        return v;
    }

    // 结构性类型匹配（简单递归比对，不强依赖具体对象地址）
    static bool typeStructurallyMatch(Type* a, Type* b) {
        if (a == b) return true;
        if (!a || !b) return false;
        if (a->GetTypeEnum() != b->GetTypeEnum()) return false;
        if (a->GetTypeEnum() == IR_DataType::IR_ARRAY) {
            auto aa = dynamic_cast<ArrayType*>(a);
            auto bb = dynamic_cast<ArrayType*>(b);
            if (!aa || !bb) return false;
            if (aa->GetNum() != bb->GetNum()) return false;
            auto sa = dynamic_cast<HasSubType*>(a);
            auto sb = dynamic_cast<HasSubType*>(b);
            if (!sa || !sb) return false;
            return typeStructurallyMatch(sa->GetSubType(), sb->GetSubType());
        }
        if (a->GetTypeEnum() == IR_DataType::IR_PTR) {
            auto sa = dynamic_cast<HasSubType*>(a);
            auto sb = dynamic_cast<HasSubType*>(b);
            if (!sa || !sb) return false;
            return typeStructurallyMatch(sa->GetSubType(), sb->GetSubType());
        }
        return true;
    }

    // 计算 dims 的乘积，带溢出检测
    static bool computeProduct(const std::vector<int>& dims, long long &outProd) {
        long long prod = 1;
        for (int d : dims) {
            if (d <= 0) return false;
            if (prod > std::numeric_limits<long long>::max() / d) return false;
            prod *= (long long)d;
        }
        outProd = prod;
        return true;
    }

public:
    GepFlatten(Function *_f) : func(_f) {}
    bool run() {
        bool modified = false;
        std::vector<GepInst *> toReplace;

        // 收集候选：如果基底（剥离 bitcast 后）是参数或 alloca，且基底类型指向数组
        for (BasicBlock *bb = func->GetFront(); bb; bb = bb->GetNextNode()) {
            for (Instruction *inst : *bb) {
                if (auto gep = dynamic_cast<GepInst *>(inst)) {
                    Value *rawbase = gep->GetOperand(0);
                    if (!rawbase) continue;
                    Value *base = stripBitCasts(rawbase);
                    if (!base) continue;
                    Type *baset = base->GetType();
                    if (!baset) continue;

                    if ((base->isParam() || dynamic_cast<AllocaInst *>(base)) &&
                        dynamic_cast<HasSubType *>(baset) &&
                        baset->GetTypeEnum() == IR_DataType::IR_PTR) {
                        Type *sub = dynamic_cast<HasSubType *>(baset)->GetSubType();
                        if (sub && sub->GetTypeEnum() == IR_DataType::IR_ARRAY) {
                            toReplace.push_back(gep);
                        }
                    }
                }
            }
        }

        DBG("Collected GEP candidates: " << toReplace.size());

        // 处理候选
        for (auto *gep : toReplace) {
            // 原始 base（可能是 bitcast 包裹的）
            Value *origBase = gep->GetOperand(0);
            if (!origBase) continue;
            // 剥离 bitcast 层找到真实 base
            Value *base = stripBitCasts(origBase);
            if (!base) continue;

            Type *baset = base->GetType();
            if (!baset) continue;
            if (baset->GetTypeEnum() == IR_DataType::IR_PTR) {
                if (auto has = dynamic_cast<HasSubType *>(baset))
                    baset = has->GetSubType();
            }

            std::vector<int> dims;
            collectArrayDims(baset, dims);
            if (dims.empty()) { DBG("skip: no array dims"); continue; }

            int numOperands = gep->GetOperandNums();
            if (numOperands <= 1) { DBG("skip: no indices"); continue; }

            std::vector<Value *> idxVals;
            for (int i = 1; i < numOperands; ++i)
                idxVals.push_back(gep->GetOperand(i));
            if (idxVals.empty()) { DBG("skip: idxVals empty"); continue; }

            // 判定首个索引是否为0（常见GEP首索引）
            size_t start = 0;
            if (!idxVals.empty()) {
                if (auto c0_int = dynamic_cast<ConstIRInt *>(idxVals[0])) {
                    if (c0_int->GetVal() == 0) start = 1;
                } else if (auto c0_data = dynamic_cast<ConstantData *>(idxVals[0])) {
                    if (c0_data->isConstZero()) start = 1;
                }
            }

            size_t idxCount = idxVals.size() >= start ? idxVals.size() - start : 0;
            if (idxCount == 0) { DBG("skip: idxCount 0"); continue; }
            if (dims.size() < idxCount) { DBG("skip: dims < idxCount"); continue; }

            // 检查索引是否全为常量
            std::vector<int> idxIntVals;
            idxIntVals.reserve(idxCount);
            bool allConst = true;
            for (size_t i = 0; i < idxCount; ++i) {
                Value *v = idxVals[start + i];
                if (auto cint = dynamic_cast<ConstIRInt *>(v)) {
                    idxIntVals.push_back(cint->GetVal());
                } else if (auto cdata = dynamic_cast<ConstantData *>(v)) {
                    if (cdata->isConstZero())
                        idxIntVals.push_back(0);
                    else { allConst = false; break; }
                } else {
                    allConst = false;
                    break;
                }
            }
            if (!allConst) { DBG("skip: not all const indices"); continue; }

            // 计算线性索引（去掉对dims的start偏移）
            long long linear = 0;
            bool linearOverflow = false;
            for (size_t i = 0; i < idxCount; ++i) {
                long long term = idxIntVals[i];
                for (size_t j = i + 1; j < idxCount; ++j) {
                    // 检查乘法溢出
                    if (term != 0 && std::llabs(term) > std::llabs(std::numeric_limits<long long>::max() / dims[j])) {
                        linearOverflow = true; break;
                    }
                    term *= dims[j];
                }
                if (linearOverflow) break;
                if (linear != 0 && std::llabs(linear) > std::llabs(std::numeric_limits<long long>::max() - term)) {
                    linearOverflow = true; break;
                }
                linear += term;
            }
            if (linearOverflow) { DBG("skip: linear overflow"); continue; }

            // 计算总元素数，用于越界检查（同样去掉start偏移）
            long long totalElements = 1;
            bool totalOverflow = false;
            for (size_t i = 0; i < idxCount; ++i) {
                if (totalElements != 0 && std::llabs(totalElements) > std::llabs(std::numeric_limits<long long>::max() / dims[i])) {
                    totalOverflow = true; break;
                }
                totalElements *= dims[i];
            }
            if (totalOverflow) { DBG("skip: totalElements overflow"); continue; }

            if (linear < 0 || linear >= totalElements) { DBG("skip: linear out of range"); continue; }
            if (linear > std::numeric_limits<int>::max()) { DBG("skip: linear > int_max"); continue; }

            // 构造新索引，保持首个0（如果存在）
            std::vector<Value *> newIndices;
            if (start == 1) newIndices.push_back(createConstInt(0));
            newIndices.push_back(createConstInt(static_cast<int>(linear)));

            // 如果是一维数组且单索引（最简单情况），直接替换（不需要 bitcast）
            if (dims.size() == 1 && idxCount == 1) {
                DBG("Perform simple flatten (1D array)");
                GepInst *newgep = createRawGep(origBase, newIndices); 
                if (!newgep) continue;

                Type *oldType = gep->GetType();
                if (!oldType) { delete newgep; continue; }

                Type *newType = newgep->GetType();
                if (!newType) { delete newgep; continue; }

                if (!typeStructurallyMatch(newType, oldType)) {
                    DBG("type mismatch in simple flatten, skip");
                    delete newgep;
                    continue;
                }

                BasicBlock *bb = gep->GetParent();
                if (!bb) { delete newgep; continue; }
                bool placed = false;
                for (auto it = bb->begin(); it != bb->end(); ++it) {
                    if (*it == gep) { it.InsertBefore(newgep); placed = true; break; }
                }
                if (!placed) bb->push_back(newgep);

                gep->ReplaceAllUseWith(newgep);
                gep->ClearRelation();
                gep->EraseFromManager();
                modified = true;
                continue;
            }

            // 多维合并：需要 bitcast（把基底类型从 N 维数组指针转换成一维数组指针）
            DBG("Attempting bitcast flatten: dims.size=" << dims.size() << " idxCount=" << idxCount);

            // 仅当完全索引到元素（idxCount == dims.size()）时才做 bitcast 合并
            if (idxCount != dims.size()) {
                DBG("skip: not fully indexed to element (need idxCount == dims.size())");
                continue;
            }

            long long prod = 0;
            if (!computeProduct(dims, prod)) {
                DBG("skip: cannot compute product of dims (overflow or invalid)");
                continue;
            }
            if (prod > static_cast<long long>(std::numeric_limits<int>::max())) {
                DBG("skip: product too large for int array length: " << prod);
                continue;
            }

            // 找到最底层元素类型（elemType）
            Type *elemType = baset;
            while (elemType && elemType->GetTypeEnum() == IR_DataType::IR_ARRAY) {
                if (auto ht = dynamic_cast<HasSubType *>(elemType)) elemType = ht->GetSubType();
                else break;
            }
            if (!elemType) { DBG("skip: cannot determine elemType"); continue; }

            // 构造 flat array type 和 pointer type：用你们的工厂函数
            ArrayType *flatArrType = ArrayType::NewArrayTypeGet(static_cast<int>(prod), elemType);
            if (!flatArrType) { DBG("skip: cannot make flat array type"); continue; }
            Type *castPtrType = PointerType::NewPointerTypeGet(flatArrType);
            if (!castPtrType) { DBG("skip: cannot make cast ptr type"); continue; }

            // 在原 GEP 所在 BB 的插入点前插入 bitcast(origBase -> castPtrType)
            BasicBlock *bb = gep->GetParent();
            if (!bb) { DBG("skip: gep has no parent"); continue; }

            BitCastInst *bitcast = new BitCastInst((Operand)origBase, castPtrType);
            // 插入 bitcast 到 BB，放到原 gep 之前（确保执行顺序）
            bool bc_placed = false;
            for (auto it = bb->begin(); it != bb->end(); ++it) {
                if (*it == gep) { it.InsertBefore(bitcast); bc_placed = true; break; }
            }
            if (!bc_placed) bb->push_back(bitcast);

            // 用 bitcast 作为 base 构造新的 GEP
            GepInst *newgep = createRawGep((Value*)bitcast, newIndices);
            if (!newgep) {
                DBG("fail: newgep null after bitcast");
                // 不删除 bitcast，留给后续 pass 或手动清理以避免破坏 IR 管理器状态
                continue;
            }

            // 校验 newgep 类型与 oldType 是否结构兼容
            Type *oldType = gep->GetType();
            Type *newType = newgep->GetType();
            if (!newType) {
                DBG("newgep has no type, skip replacing (conservative)");
                delete newgep;
                continue;
            }
            if (!typeStructurallyMatch(newType, oldType)) {
                DBG("typeStructurallyMatch failed, skip replacing");
                delete newgep;
                continue;
            }

            // 插入 newgep 并替换原 GEP
            bool placed = false;
            for (auto it = bb->begin(); it != bb->end(); ++it) {
                if (*it == gep) { it.InsertBefore(newgep); placed = true; break; }
            }
            if (!placed) bb->push_back(newgep);

            gep->ReplaceAllUseWith(newgep);

            // 删除旧 GEP，并保持 bitcast
            gep->ClearRelation();
            gep->EraseFromManager();

            modified = true;
        }

        DBG("GepFlatten finished, modified=" << (modified ? 1 : 0));
        return modified;
    }
};