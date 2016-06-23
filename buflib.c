#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#if 0
/*
** Read an integer numeral from string 'fmt' or return 'df' if
** there is no numeral
*/
static int digit (int c) { return '0' <= c && c <= '9'; }

static int getnum (const char **fmt, int df) {
  if (!digit(**fmt))  /* no number? */
    return df;  /* return default value */
  else {
    int a = 0;
    do {
      a = a*10 + (*((*fmt)++) - '0');
    } while (digit(**fmt) && a <= ((int)MAXSIZE - 9)/10);
    return a;
  }
}


/*
** Read an integer numeral and raises an error if it is larger
** than the maximum size for integers.
*/
static int getnumlimit (Header *h, const char **fmt, int df) {
  int sz = getnum(fmt, df);
  if (sz > MAXINTSIZE || sz <= 0)
    luaL_error(h->L, "integral size (%d) out of limits [1,%d]",
                     sz, MAXINTSIZE);
  return sz;
}

#endif

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
  const char * buffer;        // buffer
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

enum kOperation next_operation(struct State * st);

enum kOperation next_operation(struct State * st) {

  const char * pc = st->instructions + st->offset;

  char c;

start:

  switch((c = *pc++)) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
      goto start;
    case '>':
      st->is_little = 0;
      goto start;
    case '<':
      st->is_little = 1;
      goto start;
    case '=':
      st->is_little = 1;
      goto start;
    case '$':
    case '@':
    case '%':
    case '(':
    case ')':
    case '+':
    case '-':
    case '*':
    case '/':
    case 'b':
    case 'i':
    case 'u':
    case 'f':
    case 'd':
    case 's':
      break;
  }
  return k_hlt;
}

#define INIT_MEM_SIZE 32

static int buf_read (lua_State *L) {
  struct State st = {
    .index = 0, .offset = 0, .repeat = 0, .is_eval = 0,
    .ret = 0, .L = L, .is_little = 1,
  };

  st.buffer = luaL_checklstring(L, 1, &st.buffer_size);
  st.instructions = luaL_checkstring(L, 2);
  st.variable = malloc(sizeof(int64_t) * INIT_MEM_SIZE);
  if (!st.variable) {
    return luaL_error(L, "malloc failed");
  }
  st.variable[0] = INIT_MEM_SIZE;

  //'< u4 +12 i4'

  enum kOperation op;
  while ((op = next_operation(&st)) != k_hlt) {
    switch (op) {
      case k_bool:
        lua_pushboolean(L, (int)st.ivalue);
        break;
      case k_int:
        lua_pushinteger(L, (lua_Integer)st.ivalue);
        break;
      case k_float:
        lua_pushnumber(L, st.fvalue);
        break;
      case k_string:
        lua_pushstring(L, st.svalue);
        break;
      case k_lstring:
        lua_pushlstring(L, st.svalue, st.len);
        break;
    }
  }

  free(st.variable);
  return st.ret;
}

static const luaL_Reg buffer_lib[] = {
  {"read", buf_read},
  {NULL, NULL}
};

LUA_API int luaopen_buffer (lua_State *L) {
  luaL_newlib(L, buffer_lib);
  return 1;
}

