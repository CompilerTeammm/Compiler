#ifndef __POSITION_H__
#define __POSITION_H__
#include <string>
struct loc
{
public:
  // 用于报错提示
  int line = 0; // 行号
  int col = 0;  // 列号

  std::string toString() const
  {
    return "Line: " + std::to_string(line) + ", Column: " + std::to_string(col);
  }
};

#endif