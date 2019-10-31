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
  vector:
  string: utf-8 7-bit ascii, 其他类型的字符串应该用[ubyte] [uint]表示
  reference to enum, struct, union
*/


/*

 > 大端, < 小端

 $[类型描述] 创建中间变量，通过 $n 使用，不作为结果返回
 &[类型描述] 引用，此值后续可以通过 $n 来引用,同时也作为结果返回
 $n 访问变量
 +[n|%n|()] 当前指针向前移动n个字节
 -[n|%n|()] 当前指针向后移动n个字节

 = 本次读不移动指针

 * 重复读标记, 下一项操作 重复n次 n >= 1, 当结果没有放入table时,当前限制最大值127
 *A: 数值字面量
 *$B: 临时栈空间中的变量. $1, $2
 *[C]: Lua数值表达式, 支持使用临时栈空间变量. *[ $1 + 2]


 { 括号中的内容保存到table中,不能嵌套table }

 [@ or ^] 返回当前位置相当于buffer首地址的偏移
 b[n] bool值 默认1
 i[n] 有符号整数 默认4
 u[n] 无符号整数 默认4
 f float 32
 d float 64, double
 s[n] 字符串 没有选项n时是零结尾字符串，存在n时，固定长度字符串, n: [1 - 4]
 c[n] 指定长度的字符串 n [1 - 2^32]

*/

struct State {
  const char * buffer;        // buffer 首地址
  const char * pointer;       // 当前指针位置
  size_t       buffer_size;   // buffer size, 当直接传递地址,此值为0时,不检查边界安全
  const char * instructions;  // 指令列表

  int64_t    * variable;      // 变量数组
  int          index;         // 变量序号
  int          create_var;    // $ 中间变量,不作为结果返回
  int          create_ref;    // & 可以引用,作为结果返回
  int          dont_move;     // =

  int          in_tb;         // {}
  int          tb_idx;        // table index

  int          little;        // 小端标记
  uint32_t     repeat;        // 重复标记
  lua_State  * L;             // userdata
  int          stack_space;   // check_stack
  int          ret;           // 结果数量
};

