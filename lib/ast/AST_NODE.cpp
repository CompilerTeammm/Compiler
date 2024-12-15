#include "ast/AST_NODE.hpp"
#include <math.h>
void ConstDeclAST::HandleSymbol() const
{
    if (this->Type == VarType::INT || this->Type == VarType::FLOAT) // 支持 int 和 float
    {
        for (auto &it : this->ConstDefVec)
        {
            auto DefAstPtr = reinterpret_cast<VarDefAST *>(it);
            
            if (DefAstPtr->DimSizeVec.size() == 0)
            {
                // 标量变量
                if (SymbolTable::FindLocalScopeSymbol(*DefAstPtr->VarIdent))
                {
                    exit(-1);
                }

                if (this->Type == VarType::INT)
                {
                    // 处理 int 类型常量
                    int initval;
                    if (DefAstPtr->InitValue->keys[0]->GetConstVal(initval))
                    {
                        exit(-1);
                    }
                    Symbol *sym = new Symbol(true, true, initval);
                    SymbolTable::AddSymbol(*DefAstPtr->VarIdent, sym);
                    DefAstPtr->VarSym = sym;
                }
                else if (this->Type == VarType::FLOAT)
                {
                    // 处理 float 类型常量
                    float initval;
                    if (DefAstPtr->InitValue->keys[0]->GetConstVal(initval))
                    {
                        exit(-1);
                    }
                    Symbol *sym = new Symbol(true, true, initval);
                    SymbolTable::AddSymbol(*DefAstPtr->VarIdent, sym);
                    DefAstPtr->VarSym = sym;
                }
            }
            else
            {
                // 数组类型
                std::vector<int> ArrayDimVec;
                for (auto &it : this->ConstDefVec)
                {
                    auto ConstDefPtr = reinterpret_cast<VarDefAST *>(it);
                    // 检查符号是否已定义
                    if (SymbolTable::FindLocalScopeSymbol(*ConstDefPtr->VarIdent))
                    {
                        exit(-1);
                    }

                    InitValTree<int> *initval = NULL;
                    if (ConstDefPtr->InitValue && ConvertTreeToInt(ConstDefPtr->InitValue, initval))
                    {
                        exit(-1);
                    }

                    std::vector<int> dim;
                    for (auto &it : ConstDefPtr->DimSizeVec)
                    {
                        int temp;
                        if (it->GetConstVal(temp))
                        {
                            exit(-1);
                        }
                        dim.push_back(temp);
                    }

                    if (this->Type == VarType::INT)
                    {
                        // int 类型数组处理
                        Symbol *sym = new Symbol(true, dim, initval);
                        SymbolTable::AddSymbol(*DefAstPtr->VarIdent, sym);
                        ConstDefPtr->VarSym = sym;
                    }
                    else if (this->Type == VarType::FLOAT)
                    {
                        // float 类型数组处理
                        InitValTree<float> *initval = NULL;
                        std::vector<float> floatDimVec;
                        for (size_t i = 0; i < dim.size(); ++i)
                        {
                            // Convert dimension size from int to float
                            floatDimVec.push_back(static_cast<float>(dim[i]));
                        }
                        
                        Symbol *sym = new Symbol(true, floatDimVec, initval);
                        SymbolTable::AddSymbol(*DefAstPtr->VarIdent, sym);
                        ConstDefPtr->VarSym = sym;
                    }
                }
            }
        }
    }
    else
    {
        exit(-1);
    }
}


void VarDeclAST::HandleSymbol() const
{
    if (this->Type == VarType::INT || this->Type == VarType::FLOAT) // 支持 INT 和 FLOAT
    {
        for (auto &it : this->VarDefVec)
        {
            auto DefAstPtr = reinterpret_cast<VarDefAST *>(it);
            if (DefAstPtr->DimSizeVec.size() == 0) // 非数组
            {
                if (SymbolTable::FindLocalScopeSymbol(*DefAstPtr->VarIdent))
                {
                    exit(-1); // 标识符重复定义
                }

                Symbol *sym;
                if (this->Type == VarType::INT)
                {
                    sym = new Symbol(false, true, 0); // 创建 int 类型符号
                }
                else if (this->Type == VarType::FLOAT)
                {
                    sym = new Symbol(false, true, 0.0f); // 创建 float 类型符号
                }
                SymbolTable::AddSymbol(*DefAstPtr->VarIdent, sym);
                DefAstPtr->VarSym = sym;

                bool IsInit = !!DefAstPtr->InitValue;
                bool IsConstInit;
                bool IsGlobal = sym->GetGlobalFlag();

                if (IsInit)
                {
                    sym->VarArrtributes.IsInited = true;
                    if (this->Type == VarType::INT)
                    {
                        InitValTree<int> *IntTree;
                        IsConstInit = !ConvertTreeToInt(DefAstPtr->InitValue, IntTree);
                        if (IsGlobal)
                        {
                            if (!IsConstInit)
                            {
                                exit(-1);
                            }
                            sym->VarArrtributes.InitVal = IntTree->FindFirst();
                        }
                        else
                        {
                            sym->VarArrtributes.ExpVal = DefAstPtr->InitValue->FindFirst();
                        }
                    }
                    else if (this->Type == VarType::FLOAT)
                    {
                        InitValTree<float> *FloatTree;
                        IsConstInit = !ConvertTreeToFloat(DefAstPtr->InitValue, FloatTree);
                        if (IsGlobal)
                        {
                            if (!IsConstInit)
                            {
                                exit(-1);
                            }
                            sym->VarArrtributes.InitVal = FloatTree->FindFirst();
                        }
                        else
                        {
                            sym->VarArrtributes.ExpVal = DefAstPtr->InitValue->FindFirst();
                        }
                    }
                }
                else
                {
                    sym->VarArrtributes.IsInited = false;
                }
            }
            else // 数组
            {
                for (auto &it : this->VarDefVec)
                {
                    auto ConstDefPtr = reinterpret_cast<VarDefAST *>(it);
                    if (SymbolTable::FindSymbol(*ConstDefPtr->VarIdent))
                    {
                        exit(-1); // 标识符重复定义
                    }

                    std::vector<int> dim;
                    for (auto &it : ConstDefPtr->DimSizeVec)
                    {
                        if (this->Type == VarType::INT)
                        {
                            int temp;
                            if (it->GetConstVal(temp)) // 获取数组维度
                            {
                                exit(-1); // 数组维度非法
                            }
                            dim.push_back(temp);
                        }
                        else if (this->Type == VarType::FLOAT)
                        {
                            float temp;
                            if (it->GetConstVal(temp)) // 获取数组维度
                            {
                                exit(-1); // 数组维度非法
                            }
                            dim.push_back(static_cast<int>(temp)); // 对于浮动类型，转换为 int
                        }
                    }

                    Symbol *sym;
                    if (this->Type == VarType::INT)
                    {
                        sym = new Symbol(false, dim); // 创建 int 数组符号
                    }
                    else if (this->Type == VarType::FLOAT)
                    {
                        sym = new Symbol(false, dim); // 创建 float 数组符号
                    }
                    SymbolTable::AddSymbol(*ConstDefPtr->VarIdent, sym);
                    ConstDefPtr->VarSym = sym;

                    bool IsInit = !!ConstDefPtr->InitValue;
                    bool IsConstInit;
                    bool IsGlobal = sym->GetGlobalFlag();

                    if (IsInit)
                    {
                        sym->ArrAttributes->IsInited = true;
                        if (this->Type == VarType::INT)
                        {
                            InitValTree<int> *IntTree;
                            IsConstInit = !ConvertTreeToInt(ConstDefPtr->InitValue, IntTree);
                            if (IsGlobal)
                            {
                                if (!IsConstInit)
                                {
                                    exit(-1);
                                }
                                sym->ArrAttributes->ConstInitValInt = IntTree;
                                sym->ArrAttributes->VarInitVal = NULL;
                            }
                            else
                            {
                                sym->ArrAttributes->ConstInitValInt = NULL;
                                sym->ArrAttributes->VarInitVal = ConstDefPtr->InitValue;
                            }
                        }
                        else if (this->Type == VarType::FLOAT)
                        {
                            InitValTree<float> *FloatTree;
                            IsConstInit = !ConvertTreeToFloat(ConstDefPtr->InitValue, FloatTree);
                            if (IsGlobal)
                            {
                                if (!IsConstInit)
                                {
                                    exit(-1);
                                }
                                sym->ArrAttributes->ConstInitValFloat = FloatTree;
                                sym->ArrAttributes->VarInitVal = NULL;
                            }
                            else
                            {
                                sym->ArrAttributes->ConstInitValFloat = NULL;
                                sym->ArrAttributes->VarInitVal = ConstDefPtr->InitValue;
                            }
                        }
                    }
                    else
                    {
                        sym->ArrAttributes->IsInited = false;
                    }
                }
            }
        }
    }
    else
    {
        exit(-1); // 不支持的类型
    }
}


bool PrimaryExpAST::GetConstVal(int &val) const
{
    if (this->tp == PrimaryType::NumInt)
    {
        return reinterpret_cast<NumberAST *>(this->number)->GetConstVal(val);
    }
    // else if (this->tp == PrimaryType::NumFloat)
    // {
    //     float temp;
    //     bool result = reinterpret_cast<NumberAST *>(this->number)->GetConstVal(temp);
    //     val = static_cast<int>(temp); // Implicit conversion, may lose precision
    //     return result;
    // }
    else if (this->tp == PrimaryType::Exp)
    {
        return reinterpret_cast<ExpAST *>(this->exp)->GetConstVal(val);
    }
    else
    {
        return reinterpret_cast<LValAST *>(this->lval)->GetConstVal(val);
    }
}

bool PrimaryExpAST::GetConstVal(float &val) const
{
    if (this->tp == PrimaryType::NumFloat)
    {
        return reinterpret_cast<NumberAST *>(this->number)->GetConstVal(val);
    }
    // else if (this->tp == PrimaryType::NumInt)
    // {
    //     int temp;
    //     bool result = reinterpret_cast<NumberAST *>(this->number)->GetConstVal(temp);
    //     val = static_cast<float>(temp);
    //     return result;
    // }
    else if (this->tp == PrimaryType::Exp)
    {
        return reinterpret_cast<ExpAST *>(this->exp)->GetConstVal(val);
    }
    else
    {
        return reinterpret_cast<LValAST *>(this->lval)->GetConstVal(val);
    }
}

bool UnaryExpAST::GetConstVal(int &val) const
{
    if (tp == ExpType::Primary)
    {
        return reinterpret_cast<PrimaryExpAST *>(this->primary_exp)->GetConstVal(val);
    }
    else if (tp == ExpType::OpExp)
    {
        return reinterpret_cast<UnaryExpAST *>(this->unary_exp)->GetConstVal(val);
    }
    else if (tp == ExpType::Call)
    {
        return true;
    }
    else
    {
        return true;
    }
}

bool UnaryExpAST::GetConstVal(float &val) const
{
    if (tp == ExpType::Primary)
    {
        return reinterpret_cast<PrimaryExpAST *>(this->primary_exp)->GetConstVal(val);
    }
    else if (tp == ExpType::OpExp)
    {
        return reinterpret_cast<UnaryExpAST *>(this->unary_exp)->GetConstVal(val);
    }
    else if (tp == ExpType::Call)
    {
        return true;
    }
    else
    {
        return true;
    }
}

bool MulExpAST::GetConstVal(int &val) const
{
    int temp, res;
    bool flag;
    flag = reinterpret_cast<UnaryExpAST *>(this->unary_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;
    for (int i = 1; i < unary_exp.size(); ++i)
    {
        if (this->op[i - 1] == "*")
        {
            flag = reinterpret_cast<UnaryExpAST *>(this->unary_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res *= temp;
        }
        else if (this->op[i - 1] == "/")
        {
            flag = reinterpret_cast<UnaryExpAST *>(this->unary_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res /= temp;
        }
        else if (this->op[i - 1] == "%")
        {
            flag = reinterpret_cast<UnaryExpAST *>(this->unary_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res %= temp;
        }
        else
        {
            return true;
        }
    }
    val = res;
    return false;
}

bool MulExpAST::GetConstVal(float &val) const
{
    float temp, res;
    bool flag;
    
    // 获取第一个 UnaryExp 的常量值
    flag = reinterpret_cast<UnaryExpAST *>(this->unary_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;

    // 进行乘法、除法和取余操作（浮点数处理）
    for (int i = 1; i < unary_exp.size(); ++i)
    {
        flag = reinterpret_cast<UnaryExpAST *>(this->unary_exp[i])->GetConstVal(temp);
        if (flag)
            return true;

        if (this->op[i - 1] == "*")
        {
            res *= temp;
        }
        else if (this->op[i - 1] == "/")
        {
            if (temp == 0.0f) // 防止除以零
                return true;
            res /= temp;
        }
        else if (this->op[i - 1] == "%")
        {
            if (temp == 0.0f) // 防止除以零
                return true;
            res = std::fmod(res, temp); // 使用 fmod 来处理浮点数的取余
        }
        else
        {
            return true;
        }
    }
    val = res;  // 返回浮点数结果
    return false;
}

bool AddExpAST::GetConstVal(int &val) const
{
    int temp, res;
    bool flag;
    flag = reinterpret_cast<MulExpAST *>(this->mul_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;
    for (int i = 1; i < mul_exp.size(); ++i)
    {
        if (this->op[i - 1] == "+")
        {
            flag = reinterpret_cast<MulExpAST *>(this->mul_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res += temp;
        }
        else if (this->op[i - 1] == "-")
        {
            flag = reinterpret_cast<MulExpAST *>(this->mul_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res -= temp;
        }
        else
        {
            return true;
        }
    }
    val = res;
    return false;
}

bool AddExpAST::GetConstVal(float &val) const
{
    float temp, res;
    bool flag;
    
    // 获取第一个 MulExp 的常量值
    flag = reinterpret_cast<MulExpAST *>(this->mul_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;

    // 进行加法和减法（浮点数处理）
    for (int i = 1; i < mul_exp.size(); ++i)
    {
        flag = reinterpret_cast<MulExpAST *>(this->mul_exp[i])->GetConstVal(temp);
        if (flag)
            return true;

        if (this->op[i - 1] == "+")
        {
            res += temp;
        }
        else if (this->op[i - 1] == "-")
        {
            res -= temp;
        }
        else
        {
            return true;
        }
    }
    val = res;  // 返回浮点数的结果
    return false;
}

bool RelExpAST::GetConstVal(int &val) const
{
    int temp, res;
    bool flag;
    flag = reinterpret_cast<AddExpAST *>(this->add_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;
    for (int i = 1; i < add_exp.size(); ++i)
    {
        if (this->op[i - 1] == ">")
        {
            flag = reinterpret_cast<AddExpAST *>(this->add_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res = res > temp;
        }
        else if (this->op[i - 1] == ">=")
        {
            flag = reinterpret_cast<AddExpAST *>(this->add_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res = res >= temp;
        }
        else if (this->op[i - 1] == "<")
        {
            flag = reinterpret_cast<AddExpAST *>(this->add_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res = res < temp;
        }
        else if (this->op[i - 1] == "<=")
        {
            flag = reinterpret_cast<AddExpAST *>(this->add_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res = res <= temp;
        }
        else
        {
            return true;
        }
    }
    val = res;
    return false;
}

bool RelExpAST::GetConstVal(float &val) const
{
    float temp, res;
    bool flag;
    const float epsilon = 1e-6f;  // 精度误差容限

    // 获取第一个 AddExp 的常量值
    flag = reinterpret_cast<AddExpAST *>(this->add_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;

    // 进行大于、小于、等于等判断
    for (int i = 1; i < add_exp.size(); ++i)
    {
        flag = reinterpret_cast<AddExpAST *>(this->add_exp[i])->GetConstVal(temp);
        if (flag)
            return true;

        if (this->op[i - 1] == ">")
        {
            res = (res > temp) ? 1.0f : 0.0f;
        }
        else if (this->op[i - 1] == ">=")
        {
            res = (res >= temp) ? 1.0f : 0.0f;
        }
        else if (this->op[i - 1] == "<")
        {
            res = (res < temp) ? 1.0f : 0.0f;
        }
        else if (this->op[i - 1] == "<=")
        {
            res = (res <= temp) ? 1.0f : 0.0f;
        }
        else
        {
            return true;
        }
    }

    val = res;  // 最终结果（1.0f 或 0.0f）
    return false;
}

bool EqExpAST::GetConstVal(int &val) const
{
    int temp, res;
    bool flag;
    flag = reinterpret_cast<RelExpAST *>(this->rel_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;
    for (int i = 1; i < rel_exp.size(); ++i)
    {
        if (this->op[i - 1] == "==")
        {
            flag = reinterpret_cast<RelExpAST *>(this->rel_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res = res == temp;
        }
        else if (this->op[i - 1] == "!=")
        {
            flag = reinterpret_cast<RelExpAST *>(this->rel_exp[i])->GetConstVal(temp);
            if (flag)
                return true;
            res = res != temp;
        }
        else
        {
            return true;
        }
    }
    val = res;
    return false;
}

bool EqExpAST::GetConstVal(float &val) const
{
    float temp, res;
    bool flag;
    const float epsilon = 1e-6f;  // 精度误差容限

    // 获取第一个 RelExp 的常量值
    flag = reinterpret_cast<RelExpAST *>(this->rel_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;

    // 进行后续的相等或不相等判断
    for (int i = 1; i < rel_exp.size(); ++i)
    {
        flag = reinterpret_cast<RelExpAST *>(this->rel_exp[i])->GetConstVal(temp);
        if (flag)
            return true;

        if (this->op[i - 1] == "==")
        {
            // 处理浮点数相等性检查，考虑精度误差
            res = (fabs(res - temp) < epsilon) ? 1.0f : 0.0f;
        }
        else if (this->op[i - 1] == "!=")
        {
            // 处理浮点数不相等性检查，考虑精度误差
            res = (fabs(res - temp) >= epsilon) ? 1.0f : 0.0f;
        }
        else
        {
            return true;  // 其他操作符，无法处理
        }
    }
    
    val = res;  // 最终结果（1.0f 或 0.0f）
    return false;
}

bool LAndExpAST::GetConstVal(int &val) const
{
    int temp, res;
    bool flag;
    flag = reinterpret_cast<EqExpAST *>(this->eq_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;
    for (int i = 1; i < eq_exp.size(); ++i)
    {
        flag = reinterpret_cast<EqExpAST *>(eq_exp[i])->GetConstVal(temp);
        if (flag)
            return true;
        res = res && temp;
    }
    val = res;
    return false;
}

bool LAndExpAST::GetConstVal(float &val) const
{
    float temp, res;
    bool flag;
    
    // 获取第一个 EqExp 的常量值并转换为 float
    flag = reinterpret_cast<EqExpAST *>(this->eq_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = (temp != 0.0f) ? 1.0f : 0.0f;  // 非零转换为 1.0，零转换为 0.0
    
    // 对后续的 EqExp 进行逻辑运算（与运算）
    for (int i = 1; i < eq_exp.size(); ++i)
    {
        flag = reinterpret_cast<EqExpAST *>(eq_exp[i])->GetConstVal(temp);
        if (flag)
            return true;
        res = (res != 0.0f && temp != 0.0f) ? 1.0f : 0.0f;  // 逻辑与运算
    }
    
    val = res;
    return false;
}

bool LOrExpAST::GetConstVal(int &val) const
{
    int temp, res;
    bool flag;
    flag = reinterpret_cast<LAndExpAST *>(this->land_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = temp;
    for (int i = 1; i < land_exp.size(); ++i)
    {
        flag = reinterpret_cast<LAndExpAST *>(land_exp[i])->GetConstVal(temp);
        if (flag)
            return true;
        res = res || temp;
    }
    val = res;
    return false;
}

bool LOrExpAST::GetConstVal(float &val) const
{
    float temp, res;
    bool flag;
    
    // 获取第一个 LAndExp 的常量值并转换为 float
    flag = reinterpret_cast<LAndExpAST *>(this->land_exp[0])->GetConstVal(temp);
    if (flag)
        return true;
    res = (temp != 0.0f) ? 1.0f : 0.0f;  // 非零转换为 1.0，零转换为 0.0
    
    // 对后续的 LAndExp 进行逻辑运算
    for (int i = 1; i < land_exp.size(); ++i)
    {
        flag = reinterpret_cast<LAndExpAST *>(land_exp[i])->GetConstVal(temp);
        if (flag)
            return true;
        res = (res != 0.0f || temp != 0.0f) ? 1.0f : 0.0f;  // 逻辑或运算
    }
    
    val = res;
    return false;
}

bool ExpAST::GetConstVal(int &val) const
{
    return reinterpret_cast<LOrExpAST *>(this->lor_exp)->GetConstVal(val);
}

bool ExpAST::GetConstVal(float &val) const
{
    // 假设 lor_exp 是一个指向 LOrExpAST 的指针，可以调用其 GetConstVal 方法
    return reinterpret_cast<LOrExpAST *>(this->lor_exp)->GetConstVal(val);
}

bool LValAST::GetConstVal(int &val) const
{
    Symbol *sym = SymbolTable::FindSymbol(*this->VarIdent);
    if (sym == NULL)
    {
        return true;
    }
    if (sym->SymbolType == SymType::FuncName || sym->SymbolType == SymType::Label)
    {
        return true;
    }

    if (sym->SymbolType == SymType::INT32)
    {
        if (this->IndexVec.size() != 0)
        {
            return true;
        }
        if (sym->ConstFlag == false)
        {
            return true;
        }
        val = sym->VarArrtributes.InitVal;
        return false;
    }

    // 如果是 FLOAT32 类型，应该交给 GetConstVal(float &) 处理，不在此函数中处理
    return true;
}

bool LValAST::GetConstVal(float &val) const
{
    Symbol *sym = SymbolTable::FindSymbol(*this->VarIdent);
    if (sym == NULL)
    {
        return true;
    }
    if (sym->SymbolType == SymType::FuncName || sym->SymbolType == SymType::Label)
    {
        return true;
    }

    if (sym->SymbolType == SymType::FLOAT32)
    {
        if (this->IndexVec.size() != 0)
        {
            return true;
        }
        if (sym->ConstFlag == false)
        {
            return true;
        }
        val = sym->VarArrtributes.FloatInitVal; // 直接获取 float 类型
        return false;
    }

    return true;
}