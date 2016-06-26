#pragma once

#include <string.h>
#include <stdlib.h>
#include <alloca.h>
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

struct State {
  const char * buffer;        // buffer 首地址
  const char * pointer;       // 当前指针位置
  size_t       buffer_size;   // buffer size
  const char * instructions;  // 指令列表

  int64_t    * variable;      // 变量数组
  int          index;         // 变量序号
  int          create_ref;    // &
  int          create_var;    // $
  int          dont_move;     // =

  int          little;        // 小端标记
  uint32_t     repeat;        // 重复标记
  lua_State  * L;             // userdata
  int          stack_space;   // check_stack
  int          ret;           // 结果数量
};

static void run_instructions(struct State * st);
static uint32_t getnum (const char **s, uint32_t df);

static lua_Integer
unpackint (struct State *st, const char *str, int islittle, int size, int issigned);
