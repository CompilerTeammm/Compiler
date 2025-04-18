#include "CFG.hpp"

namespace Trival
{
  bool check_builtin(std::string id);
  User *GenerateCallInst(std::string id, std::vector<Operand> args);
  CallInst *BuildInTransform(CallInst *);
};