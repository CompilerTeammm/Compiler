#ifndef __POSITION_H__
#define __POSITION_H__
#include <string>
struct loc
{
  int line = 0;   // 行号
  int column = 0; // 列号
  std::string toString() const
  {
    return "Line: " + std::to_string(line) + ", Column: " + std::to_string(column);
  }

public:
  int begin;
  int end;
};

#endif