#pragma once

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

/*
  scalar types:
  8 bit: byte ubyte bool
  16 bit: short ushort
  32 bit: int uint float
  64 bit: long ulong double

  所有的整数类型都转换到lua_Integer,
  float/double 转换到lua_Number
  bool 转换成bool
  64位无符号类型暂时不做特殊处理

  non-scalar types:

  vector: 不支持嵌套数组,可以用table包装一层数组
  string: utf-8 7-bit ascii, 其他类型的字符串应该用[ubyte] [uint]表示
  reference to enum, struct, union

*/
enum kOperation {
  k_bool,    // boolean
  k_int,     // ubyte
  k_float,   // double
  k_string,  // string without len
  k_lstring, // string with size

  k_forward,    // 向前移动指针 uint32
  k_backward,    // 向后移动指针 uint32

  k_dereference,  // 变量解引用

  k_eval,     // 表达式求值
  k_left,     // 左括号
  k_right,    // 右括号
  k_plus,     // +
  k_minus,    // -
  k_multiply, // *
  k_divide,   // /

  k_reference,    // 创建引用 int64
  k_variable,    // 创建变量 int
  k_hlt,        // 结束
};

struct State {
  const char * buffer;        // buffer 首地址
  const char * pointer;       // 当前指针位置
  size_t       buffer_size;   // buffer size
  const char * instructions;  // 指令列表

  int64_t    * variable;      // 变量数组
  int          index;         // 变量序号

  ptrdiff_t    offset;        // buffer寻址偏移

  int64_t      ivalue; // 整型value
  double       fvalue; // 浮点值
  size_t       len;
  const char * svalue;

  int          is_little;     // 小端标记
  int          repeat;        // 重复标记
  int          is_eval;       // 是否正在求值
  lua_State  * L;             // userdata
  int          ret;           // 结果数量
};

static enum kOperation next_operation(struct State * st);
static uint32_t getnum (const char **s, uint32_t df);
static uint32_t getnum_with_limit (struct State *st, const char **s,
    uint32_t df, uint32_t min, uint32_t max);

static lua_Integer
unpackint (struct State *st, const char *str, int islittle, int size, int issigned);
