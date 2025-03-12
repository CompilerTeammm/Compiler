#ifndef __POSITION_H__
#define __POSITION_H__
#include <string>
struct loc
{
public:
  // 用于报错提示
  int line = 0; // 行号
  int col = 0;  // 列号

  int begin = 0;
  int end = 0;
  std::string toString() const
  {
    return "Line: " + std::to_string(line) + ", Column: " + std::to_string(col);
  }
  loc() : begin(0), end(0) {}

  loc(int _line) : begin(_line), end(_line) {}
  void SET(const loc &_) { *this = _; }
};

#endif