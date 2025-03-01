#pragma once
#include <memory>
#include <iostream>
#include <cassert>
#include <map>
// 类成员 构造  get&set  print&clone 其他
// 在类内实现了类型转换逻辑

enum IR_DataType
{
    IR_Value_INT,
    IR_Value_VOID,
    IR_Value_Float,
    IR_PTR,
    IR_ARRAY,
    BACKEND_PTR,
    IR_INT_64
};

class Type
{
public:
    IR_DataType type;
    size_t size;

    // 构造&析构默认
    Type(IR_DataType _type) : type(_type) {}
    virtual ~Type() = default;

    virtual int GetLayer() { return 0; }
    size_t GetSize() { return size; }
    IR_DataType GetTypeEnum() { return type; }

    virtual void print() = 0;
};

class IntType : public Type
{
    // 私有构造函数，防止外部直接实例化
    IntType() : Type(IR_Value_INT) { size = 4; }

public:
    void print() final
    {
        std::cout << "i32";
    }

    // 返回单例
    static IntType *NewIntTypeGet()
    {
        static IntType single;
        return &single;
    }

    // 类型转换：IntType -> FloatType
    FloatType *ToFloatType()
    {
        return FloatType::NewFloatTypeGet();
    }
    // 类型转换：IntType -> Int64Type
    Int64Type *ToInt64Type()
    {
        return Int64Type::NewInt64TypeGet();
    }
};

// 加入了int64的类型
class Int64Type : public Type
{
    Int64Type() : Type(IR_INT_64) { size = 8; }

public:
    void print() final
    {
        std::cout << "i64";
    }

    static Int64Type *NewInt64TypeGet()
    {
        static Int64Type single;
        return &single;
    }
};

class FloatType : public Type
{
    FloatType() : Type(IR_Value_Float) { size = 4; }

public:
    void print() final
    {
        std::cout << "f32";
    }

    static FloatType *NewFloatTypeGet()
    {
        static FloatType single;
        return &single;
    }

    // 类型转换：FloatType -> IntType
    IntType *ToIntType()
    {
        return IntType::NewIntTypeGet();
    }
};

class VoidType : public Type
{
    VoidType() : Type(IR_Value_VOID) { size = 0; }

public:
    void print() final
    {
        std::cout << "void";
    }

    static VoidType *NewVoidTypeGet()
    {
        static VoidType single;
        return &single;
    }
};

class BoolType : public Type
{
    // 看作int32=1/0
    BoolType() : Type(IR_Value_INT) { size = 4; }

public:
    void print() final
    {
        std::cout << "il"; // 类llvm
    }
    static BoolType *NewBoolTypeGet()
    {
        static BoolType single;
        return &single;
    }
};

// 具有子类型的Type：数组&指针继承自此
class HasSubType : public Type
{
protected:
    Type *subtype;
    int layer;

public:
    HasSubType(IR_DataType _type, Type *_subtype) : Type(_type), subtype(_subtype), layer(_subtype->GetLayer() + 1) {}

    Type *GetSubType() { return subtype; }
    int GetLayer() final { return layer; }
    Type *GetBaseType()
    {
        Type *basetype = nullptr;
        basetype = this;
        for (int i = 0; i < layer; i++)
        {
            if (HasSubType *hassubtype = dynamic_cast<HasSubType *>(basetype))
            {
                basetype = hassubtype->GetSubType();
            }
        }
        return basetype;
    }
};

class PointerType : public HasSubType
{
    PointerType(Type *_subtype) : HasSubType(IR_PTR, _subtype) { size = 8; }

public:
    void print() final
    {
        subtype->print();
        std::cout << "*";
    }

    static PointerType *NewPointerTypeGet(Type *_subtype)
    {
        static std::map<Type *, PointerType *> single;
        auto &tmp = single[_subtype];
        if (tmp == nullptr)
            tmp = new PointerType(_subtype);
        return tmp;
    }
};

class ArrayType : public HasSubType
{
    int num;

    ArrayType(int _num, Type *_subtype) : num(_num), HasSubType(IR_ARRAY, _subtype)
    {
        size = _num * _subtype->GetSize();
    }

public:
    int GetNum() { return num; }

    void print() final
    {
        std::cout << "[" << num << " x ";
        subtype->print();
        std::cout << "]";
    }

    static ArrayType *NewArrayTypeGet(int _num, Type *_subtype)
    {
        using Key = std::pair<int, Type *>;
        static std::map<Key, ArrayType *> single;
        auto &tmp = single[Key(_num, _subtype)];
        if (tmp == nullptr)
            tmp = new ArrayType(_num, _subtype);
        return tmp;
    }
};

Type *NewTypeFromIRDataType(IR_DataType _type)
{
    switch (_type)
    {
    case IR_DataType::IR_Value_INT:
        return IntType::NewIntTypeGet();
    case IR_DataType::IR_Value_Float:
        return FloatType::NewFloatTypeGet();
    case IR_DataType::IR_Value_VOID:
        return VoidType::NewVoidTypeGet();
    case IR_DataType::IR_PTR:
        return PointerType::NewPointerTypeGet(NewTypeFromIRDataType(IR_Value_INT));
    default:
        return nullptr;
    }
}