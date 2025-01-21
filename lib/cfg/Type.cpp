#include"../../include/lib/Type.hpp"
#include <cassert>

Type::Type(IR_DataType tp)
    :type(tp)
    { }
IR_DataType Type::GetTypeEnum() {return type;}
int Type::get_layer() {return 0;}
size_t Type::get_size() {return size;}
Type* Type::NewTypeByEnum(IR_DataType tp)
{
    switch (tp) {
        case IR_Value_INT: return IntType::NewIntTypeGet();
        default:assert(0);
    }
}



IntType::IntType()
    :Type(IR_Value_INT) 
    {size = 4;}
IntType* IntType::NewIntTypeGet()
{
    static IntType single;
    return &single;
}
void IntType::print()
{
    std::cout<<"i32";
}