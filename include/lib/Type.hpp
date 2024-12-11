#pragma once
#include<memory>
#include<iostream>

enum IR_DataType
{
    IR_Value_INT,IR_Value_VOID,IR_Value_Float,IR_PTR,IR_ARRAY,BACKEND_PTR, IR_INT_64
};

class Type
{
    IR_DataType type;
    protected:
    size_t size;
    Type(IR_DataType tp);
    public:
    static Type* NewTypeByEnum(IR_DataType tp);
    virtual ~Type() = default;
    virtual void print() = 0;
    virtual int get_layer();
    size_t get_size();
    IR_DataType GetTypeEnum();
};

// 之后可能会写很多继承关系 
class IntType:public Type
{

};