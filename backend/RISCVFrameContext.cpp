#include "../include/Backend/RISCVFrameContext.hpp"

std::string& NamedMOperand::GetName(){
    return name;
}
NamedMOperand::NamedMOperand(std::string _name,RISCVType _tp):RISCVMOperand(_tp),name(_name){;}

void NamedMOperand::print(){
    std::cout<<name;
}

OuterTag::OuterTag(std::string _name):NamedMOperand(_name,riscv_none){}
OuterTag* OuterTag::GetOuterTag(std::string _name){
    static std::unordered_map<std::string,std::unique_ptr<OuterTag>> tags;
    if(tags.find(_name)==tags.end()){
        tags[_name]=std::make_unique<OuterTag>(_name);
    }
    return tags[_name].get();
}

RISCVObject::RISCVObject(Type* _tp,std::string _name):NamedMOperand(_name,riscv_ptr),tp(_tp){}
RISCVObject::RISCVObject(std::string _name):NamedMOperand(_name,riscv_ptr){}
RISCVGlobalObject::RISCVGlobalObject(Type* _tp,std::string _name):RISCVObject(_tp,_name){
    local=false;
}
void RISCVGlobalObject::print(){
    NamedMOperand::print();
}

RISCVTempFloatObject::RISCVTempFloatObject(std::string _name):RISCVObject(FloatType::NewFloatTypeGet(),_name){
    local=true;
}
void RISCVTempFloatObject::print(){
    NamedMOperand::print();
}

RISCVFrameObject::RISCVFrameObject(Value* val):RISCVMOperand(RISCVTyper(val->GetType())){
    name =val->GetName();
    reg =new StackRegister(this,PhyRegister::PhyReg::s0,begin_addr_offsets);

    if(PointerType* pointtype=dynamic_cast<PointerType*>(val->GetType())){
      size=pointtype->GetSubType()->GetSize();
    }else if(ArrayType* arrtype=dynamic_cast<ArrayType*>(val->GetType())){
        size=arrtype->GetSize();
    }else{
        size=val->GetType()->GetSize();
    }
    contexttype=RISCVTyper(val->GetType());
}
RISCVFrameObject::RISCVFrameObject():RISCVMOperand(riscv_none){
    reg =new StackRegister(this,PhyRegister::PhyReg::s0,begin_addr_offsets);
    size=8;
}
RISCVFrameObject::RISCVFrameObject(VirRegister* val):RISCVMOperand(val->GetType()){
    reg =new StackRegister(this,PhyRegister::PhyReg::s0,begin_addr_offsets);
    size=8;
    name =val->GetName();
    contexttype=val->GetType();
}
RISCVFrameObject::RISCVFrameObject(PhyRegister* preg):RISCVMOperand(preg->GetType()){
    reg =new StackRegister(this,PhyRegister::PhyReg::s0,begin_addr_offsets);
    size=8;
    name=preg->GetName();
    contexttype=preg->GetType();
}
void RISCVFrameObject::GenerateStackRegister(int offset) {
    reg->SetOffset(offset);
}

