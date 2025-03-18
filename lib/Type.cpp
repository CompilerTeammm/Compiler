#include "../include/lib/Type.hpp"

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