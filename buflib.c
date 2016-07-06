#include "buflib.h"

#define MAX_SIZET ((size_t)(~(size_t)0))
#define MAXSIZE   (sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))

#define NB  CHAR_BIT // char 类型的位数，通常是8

//mask for one character (NB 1's)

#define MC  ((1 << NB) - 1)
#define SIZE_LUA_INTEGER   ((int)sizeof(lua_Integer)) // size of a lua_Integer

/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};


// Read an integer numeral from string 's' or return 'df' if
// there is no numeral
static uint32_t isdigit (uint32_t c) { return '0' <= c && c <= '9'; }
static uint32_t getnum (const char **s, uint32_t df) {
  if (!isdigit(**s))
    return df;  // return default value when there are no number
  else {
    uint32_t a = 0;
    do {
      a = a*10 + (*((*s)++) - '0');
    } while (isdigit(**s) && a <= ((uint32_t)MAXSIZE - 9)/10);
    return a;
  }
}

#define cf \
  "local tonumber = tonumber\n" \
  "local args, str = ...\n" \
  "str = str:gsub('%$(%d+)', function (k)\n" \
  "  return args[tonumber(k)]\n" \
  "end)\n" \
  "return load('return ' .. str, 'cf', 't', {})()\n"

static int64_t get_argument (struct State * st, const char **s, uint32_t df) {
  if (**s == '[') {
    const char * p = *s;
    luaL_loadstring(st->L, cf);
    lua_createtable(st->L, 32, 0);
    while (*((*s)++) != ']') {
      if (**s == '$') {
        ++(*s);
        uint32_t idx = getnum(s, 0);
        if (idx == 0 || idx >= st->index) luaL_error(st->L, "bad variable");
        lua_pushinteger(st->L, st->variable[idx]);
        lua_rawseti(st->L, -2, idx);
      }
    }

    lua_pushlstring(st->L, p + 1, *s - p - 2);
    lua_call(st->L, 2, 1);

    int64_t r = (int64_t)luaL_checkinteger(st->L, -1);
    lua_pop(st->L, 1);
    return r;
  } else if (**s == '$') {
    ++(*s);
    uint32_t idx = getnum(s, 0);
    if (idx == 0 || idx >= st->index) luaL_error(st->L, "bad variable");
    return st->variable[idx];
  } else {
    return getnum(s, df);
  }
}

// Read an integer numeral and raises an error if it is larger
// than the maximum size for integers.
static uint32_t
get_opt_int_size (struct State *st, const char **s,
    uint32_t df, uint32_t min, uint32_t max) {
  uint32_t num = getnum(s, df);
  if (max < num || num < min)
    luaL_error(st->L, "(%I) out of limits [%I, %I]", num, min, max);
  switch (num) {
    case 0: case 1: case 2: case 4: case 8: break;
    default: luaL_error(st->L, "invalid integer size (%I)", num);
  }
  return num;
}

// Unpack an integer with 'size' bytes and 'islittle' endianness.
// If size is smaller than the size of a Lua integer and integer
// is signed, must do sign extension (propagating the sign to the
// higher bits); if size is larger than the size of a Lua integer,
// it must check the unread bytes to see whether they do not cause an
// overflow.
static lua_Integer
unpackint (struct State *st, const char *str,
    int islittle, int size, int issigned) {
  lua_Unsigned res = 0;
  int i;
  int limit = (size  <= SIZE_LUA_INTEGER) ? size : SIZE_LUA_INTEGER;
  for (i = limit - 1; i >= 0; i--) {
    res <<= NB;
    res |= (lua_Unsigned)(unsigned char)str[islittle ? i : size - 1 - i];
  }
  if (size < SIZE_LUA_INTEGER) {  /* real size smaller than lua_Integer? */
    if (issigned) {  /* needs sign extension? */
      lua_Unsigned mask = (lua_Unsigned)1 << (size*NB - 1);
      res = ((res ^ mask) - mask);  /* do sign extension */
    }
  }
  else if (size > SIZE_LUA_INTEGER) {  /* must check unread bytes */
    int mask = (!issigned || (lua_Integer)res >= 0) ? 0 : MC;
    for (i = limit; i < size; i++) {
      if ((unsigned char)str[islittle ? i : size - 1 - i] != mask)
        luaL_error(st->L, "%d-byte integer does not fit into Lua Integer", size);
    }
  }
  return (lua_Integer)res;
}

#define INIT_MEM_SIZE 32
#define MEM_GROW_SIZE 16
#define INIT_STACK_SPACE 32
#define STACK_GROW_SPACE 16

static void check_stack_space(struct State *st) {
  if (st->ret + 2 >= st->stack_space) {
    st->stack_space += STACK_GROW_SPACE;
    luaL_checkstack(st->L, st->stack_space, "too many results");
  }
}

union Ftypes {
  float f;
  double d;
  lua_Number n;
  char buff[5 * sizeof(lua_Number)];  /* enough for any float type */
};

static void copy_with_endian (volatile char *dest, volatile const char *src,
                            int size, int islittle) {
  if (islittle == nativeendian.little) {
    while (size-- != 0)
      *(dest++) = *(src++);
  }
  else {
    dest += size - 1;
    while (size-- != 0)
      *(dest--) = *(src++);
  }
}

#define check_move_pointer(sz) \
  if (st->dont_move == 0) st->pointer += (sz); else st->dont_move = 0

static void read_boolean(struct State * st, const char **s) {
  uint32_t sz = get_opt_int_size(st, s, 1, 1, 8);

  while (st->repeat-- > 0) {
    lua_pushboolean(st->L, unpackint(st, st->pointer, st->little, sz, 0));
    check_move_pointer(sz);

    if (st->in_tb == 0) {
      st->ret++;
      check_stack_space(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_integer(struct State *st, const char **s, int is_sign) {
  uint32_t sz = get_opt_int_size(st, s, 4, 1, 8);

  if (st->create_var != 0) {
    st->create_var = 0;

    int64_t num = unpackint(st, st->pointer, st->little, sz, is_sign);
    check_move_pointer(sz);
    st->variable[st->index++] = num;
  } else {

    if (st->create_ref != 0) {
      st->create_ref = 0;

      int64_t num = unpackint(st, st->pointer, st->little, sz, is_sign);
      st->variable[st->index++] = num;
    }

    while (st->repeat-- > 0) {
      lua_pushinteger(st->L, unpackint(st, st->pointer, st->little, sz, is_sign));
      check_move_pointer(sz);

      if (st->in_tb == 0) {
        st->ret++;
        check_stack_space(st);
      } else {
        lua_rawseti(st->L, -2, st->tb_idx++);
      }
    }
  }
}

static void read_float32(struct State *st) {
  while (st->repeat-- > 0) {
    volatile union Ftypes u;
    copy_with_endian(u.buff, st->pointer, 4, st->little);
    check_move_pointer(4);
    lua_pushnumber(st->L, (lua_Number)u.f);
    if (st->in_tb == 0) {
      st->ret++;
      check_stack_space(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_float64(struct State *st) {
  while (st->repeat-- > 0) {
    volatile union Ftypes u;
    copy_with_endian(u.buff, st->pointer, 8, st->little);
    check_move_pointer(8);
    lua_pushnumber(st->L, (lua_Number)u.d);
    if (st->in_tb == 0) {
      st->ret++;
      check_stack_space(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_string(struct State *st, const char **s) {
  uint32_t sz = get_opt_int_size(st, s, 0, 0, 4);

  while (st->repeat -- > 0) {
    if (sz == 0) {
      size_t slen = strlen(st->pointer);
      lua_pushlstring(st->L, st->pointer, slen);
      check_move_pointer(slen + 1); // skip a terminal zero
    } else {
      uint32_t slen = unpackint(st, st->pointer, st->little, sz, 0);
      lua_pushlstring(st->L, st->pointer + sz, slen);
      check_move_pointer(sz + slen);
    }

    if (st->in_tb == 0) {
      st->ret++;
      check_stack_space(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_fixed_string(struct State *st, const char **s) {
  uint32_t sz = getnum(s, 1);
  if (sz == 0) luaL_error(st->L, "bad n in 'c[n]'");

  while (st->repeat-- > 0) {
    lua_pushlstring(st->L, st->pointer, sz);
    check_move_pointer(sz);
    if (st->in_tb == 0) {
      st->ret++;
      check_stack_space(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}


static void run_instructions(struct State * st) {
  const char * pc = st->instructions;
  char c;

  st->variable = alloca(sizeof(int64_t) * INIT_MEM_SIZE);
  st->variable[0] = INIT_MEM_SIZE - 1;
  st->index = 1;

  while ((c = *pc++)) {

    switch((c)) {
      case ' ': case '\t': case '\r': case '\n': goto next_loop;
      case '>': st->little = 0; goto next_loop;
      case '<': st->little = 1; goto next_loop;

      case '=': st->dont_move = 1; goto next_loop;

      case '{':
        {
          if (st->in_tb != 0) luaL_error(st->L, "nested table detected");
          st->in_tb = st->tb_idx = 1;
          st->ret++;
          check_stack_space(st);
          lua_createtable(st->L, 32, 0);
          goto next_loop;
        }
      case '}':
        {
          if (st->in_tb == 0) luaL_error(st->L, "missing corresponding {");
          st->in_tb = 0;
          goto next_loop;
        }

      case '$':
      case '&':
        {
          if (*pc != 'u' && *pc != 'i') luaL_error(st->L, "u/i expected after &");

          int64_t cur_size = st->variable[0];
          if (st->index >= cur_size) {
            int64_t * ns = alloca(sizeof(int64_t) * (cur_size + MEM_GROW_SIZE));
            memmove(ns + 1, st->variable + 1, cur_size * sizeof(int64_t));
            ns[0] = cur_size + MEM_GROW_SIZE;
            st->variable = ns;
          }
          if (c == '$') st->create_var = 1; else st->create_ref = 1;
          goto next_loop;
        }

      case '+': st->pointer += get_argument(st, &pc, 1); goto next_loop;
      case '-': st->pointer -= get_argument(st, &pc, 1); goto next_loop;
      case '*':
        {
          if (st->repeat > 1) luaL_error(st->L, "duplicate repeat flag");
          int64_t r = get_argument(st, &pc, 0);
          if (r <= 0 || r > 1024) luaL_error(st->L, "bad repeat times in '*n'");
          st->repeat = (uint32_t)r;
          goto next_loop;
        }

      case '@':
      case '^':
        {
          lua_pushinteger(st->L, st->pointer - st->buffer);
          if (st->in_tb == 0) {
            st->ret++;
            check_stack_space(st);
          } else {
            lua_rawseti(st->L, -2, st->tb_idx++);
          }
          goto next_loop;
        }

      case 'b': read_boolean(st, &pc); goto reset_repeat;
      case 'i': case 'u': read_integer(st, &pc, c == 'i'); goto reset_repeat;
      case 'f': read_float32(st); goto reset_repeat;
      case 'd': read_float64(st); goto reset_repeat;
      case 's': read_string(st, &pc); goto reset_repeat;
      case 'c': read_fixed_string(st, &pc); goto reset_repeat;
      default :
        luaL_error(st->L, "unknown read code");
    }

reset_repeat:
    st->repeat = 1;

next_loop:
    ;
  }
}

// > 大端, < 小端
//
// $n 创建中间变量，通过 $n 来使用用，不作为结果返回
// &n 引用，此值后续可以通过 $n 来引用,同时也作为结果返回
// $n 访问变量
// +[n|%n|()] 当前指针向前移动n个字节
// -[n|%n|()] 当前指针向后移动n个字节
//
// = 本次读不移动指针
// *[n|$n|()] 下一项操作 重复n次(todo 复合操作) n >= 1
//
// { 括号中的内容保存到table中 }
//
// [@ or ^] 返回当前位置相当于buffer首地址的偏移
// b[n] bool值 默认1
// i/I[n] 有符号整数 默认4
// u/U[n] 无符号整数 默认4
// f float 32
// d float 64, double
// s[n] 字符串 没有选项n时是零结尾字符串，存在n时，固定长度字符串, n: [1 - 4]
// c[n] 指定长度的字符串 n [1 - 2^32]

static int buf_read (lua_State *L) {
  struct State st = {
    .repeat = 1, .create_ref = 0, .create_var = 0, .dont_move = 0,
    .ret = 0, .L = L, .little = 1, .stack_space = INIT_STACK_SPACE,
    .in_tb = 0, .tb_idx = 0,
  };

  st.pointer = st.buffer = luaL_checklstring(L, 1, &st.buffer_size);
  st.instructions = luaL_checkstring(L, 2);

  luaL_checkstack(L, INIT_STACK_SPACE, NULL);
  run_instructions(&st);
  return st.ret;
}

static const luaL_Reg buffer_lib[] = {
  {"read", buf_read},
  {NULL, NULL}
};

LUA_API int luaopen_buffer (lua_State *L) {
  luaL_checkversion(L);
  if (sizeof(lua_Integer) < 8) {
    lua_pushstring(L, "incompatible Lua version");
  } else {
    luaL_newlib(L, buffer_lib);
  }

  return 1;
}
