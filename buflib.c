#include "buflib.inl"

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


#define isdigit(c) ('0' <= c && c <= '9')
static uint32_t read_optional_integer (const char **s, uint32_t default_value) {
  if (isdigit(**s)) {
    uint32_t a = 0;
    do {
      a = a * 10 + (*((*s)++) - '0');
    } while (isdigit(**s) && a <= ((uint32_t)MAXSIZE - 9)/10);
    return a;
  } else {
    return default_value;
  }
}

#define EXPR_CALC_LUA_SCRIPT \
  "local tonumber = tonumber\n" \
  "local args, str = ...\n" \
  "str = str:gsub('%$(%d+)', function (k)\n" \
  "  return args[tonumber(k)]\n" \
  "end)\n" \
  "return load('return ' .. str, '<expr_src>', 't', {})()\n"

// 移动指针操作(+/-)和重复操作的数值部分的表达式计算
static int64_t calc_integral_expression (struct State * st, const char **s, uint32_t df) {
  if (**s == '[') {
    const char * p = *s;
    luaL_loadstring(st->L, EXPR_CALC_LUA_SCRIPT);
    lua_createtable(st->L, 32, 0);
    while (*((*s)++) != ']') {
      if (**s == '$') {
        ++(*s);
        uint32_t idx = read_optional_integer(s, 0);
        if (idx == 0 || idx >= st->index) {
          luaL_error(st->L, "[calc_integral_expression] bad variable index: %d", idx);
        }
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
    uint32_t idx = read_optional_integer(s, 0);
    if (idx == 0 || idx >= st->index) {
      luaL_error(st->L, "[calc_integral_expression] bad variable index: %d", idx);
    }
    return st->variable[idx];
  } else {
    return read_optional_integer(s, df);
  }
}

// Read an integer numeral and raises an error if it is larger
// than the maximum size for integers.
static uint32_t
get_opt_int_size (struct State *st, const char **s,
    uint32_t df, uint32_t min, uint32_t max) {

  uint32_t num = read_optional_integer(s, df);
  if (max < num || num < min) {
    luaL_error(st->L, "(%I) out of limits [%I, %I]", num, min, max);
  }

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
  } else if (size > SIZE_LUA_INTEGER) {  /* must check unread bytes */
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

#define INIT_LUA_STACK_SPACE 32
#define LUA_STACK_GROW_SPACE 16


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

#define CHECK_MOVE_POINTER(len) \
  if (st->dont_move == 0) st->pointer += (len); else st->dont_move = 0

#define CHECK_STACK_SPACE(st) \
  if (st->ret + 2 >= st->stack_space) \
    st->stack_space += LUA_STACK_GROW_SPACE, \
    luaL_checkstack(st->L, st->stack_space, "too many results")

static void read_boolean(struct State * st, const char **s) {
  uint32_t len = get_opt_int_size(st, s, 1, 1, 8);

  if (st->buffer_size != 0 && st->pointer + st->repeat * len > st->buffer_end) {
    luaL_error(st->L, "read boolean: out of buffer");
  }
  while (st->repeat-- > 0) {
    lua_pushboolean(st->L, unpackint(st, st->pointer, st->little, len, 0));
    CHECK_MOVE_POINTER(len);

    if (st->in_tb == 0) {
      st->ret++;
      CHECK_STACK_SPACE(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_integer(struct State *st, const char **s, int is_signed) {
  uint32_t len = get_opt_int_size(st, s, 4, 1, 8);

  if (st->buffer_size != 0 && st->pointer + len * st->repeat > st->buffer_end) {
    luaL_error(st->L, "read integer: out of buffer");
  }
  int c_var = st->create_var;
  int c_ref = st->create_ref;
  st->create_ref = 0;
  st->create_var = 0;
  while (st->repeat-- > 0) {
    int64_t num = unpackint(st, st->pointer, st->little, len, is_signed);
    CHECK_MOVE_POINTER(len);

    if (c_var != 0) {
      st->variable[st->index++] = num;
    } else {
      if (c_ref != 0) {
        st->variable[st->index++] = num;
      }
      lua_pushinteger(st->L, num);
      if (st->in_tb == 0) {
        st->ret++;
        CHECK_STACK_SPACE(st);
      } else {
        lua_rawseti(st->L, -2, st->tb_idx++);
      }
    }
  }
}

static void read_float32(struct State *st) {
  if (st->buffer_size != 0 && st->pointer + 4 * st->repeat > st->buffer_end) {
    luaL_error(st->L, "read float32: out of buffer");
  }

  while (st->repeat-- > 0) {
    volatile union Ftypes u;
    copy_with_endian(u.buff, st->pointer, 4, st->little);
    CHECK_MOVE_POINTER(4);
    lua_pushnumber(st->L, (lua_Number)u.f);
    if (st->in_tb == 0) {
      st->ret++;
      CHECK_STACK_SPACE(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_float64(struct State *st) {
  if (st->buffer_size != 0 && st->pointer + 8 * st->repeat > st->buffer_end) {
    luaL_error(st->L, "read float64: out of buffer");
  }

  while (st->repeat-- > 0) {
    volatile union Ftypes u;
    copy_with_endian(u.buff, st->pointer, 8, st->little);
    CHECK_MOVE_POINTER(8); // TODO 重复读时其实没有必要检测指针移动, 优化此处
    lua_pushnumber(st->L, (lua_Number)u.d);
    // TODO 是否为数组模式也不用在重复读循环中每次检测
    if (st->in_tb == 0) {
      st->ret++;
      CHECK_STACK_SPACE(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_string(struct State *st, const char **s) {
  // s[n]
  // s: zero-terminated string
  // s[1-4]: header + string
  uint32_t len = get_opt_int_size(st, s, 0, 0, 4);

  while (st->repeat -- > 0) {
    if (len == 0) {
      size_t slen = strlen(st->pointer);
      int max_size = st->buffer_size - (st->pointer - st->buffer) - 1;
      if (st->buffer_size != 0 && slen > max_size) {
        luaL_error(st->L, "read string s[0]: out of buffer");
      }
      lua_pushlstring(st->L, st->pointer, slen);
      CHECK_MOVE_POINTER(slen + 1); // skip a terminal zero
    } else {
      uint32_t slen = unpackint(st, st->pointer, st->little, len, 0);
      if (st->buffer_size != 0 && st->pointer + len + slen > st->buffer_end) {
        luaL_error(st->L, "read string s[%d]: out of buffer", slen);
      }
      lua_pushlstring(st->L, st->pointer + len, slen);
      CHECK_MOVE_POINTER(len + slen);
    }

    if (st->in_tb == 0) {
      st->ret++;
      CHECK_STACK_SPACE(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}

static void read_fixed_string(struct State *st, const char **s) {
  uint32_t len = read_optional_integer(s, 1);
  if (len == 0) {
    luaL_error(st->L, "read fixed string c[%d]: zero", len);
  }
  if (st->buffer_size != 0 && st->pointer + len > st->buffer_end) {
    luaL_error(st->L, "read fixed string c[%d]: out of buffer", len);
  }

  while (st->repeat-- > 0) {
    lua_pushlstring(st->L, st->pointer, len);
    CHECK_MOVE_POINTER(len);
    if (st->in_tb == 0) {
      st->ret++;
      CHECK_STACK_SPACE(st);
    } else {
      lua_rawseti(st->L, -2, st->tb_idx++);
    }
  }
}


static void run_instructions(struct State * st) {
  const char * pc = st->instructions;
  char c;

  // 变量空间(栈上分配)
  // layout [剩余空间容量, ]
  // index: 栈顶下标
  st->variable = alloca(sizeof(int64_t) * INIT_MEM_SIZE);
  st->variable[0] = INIT_MEM_SIZE - 1;
  st->index = 1;

  while ((c = *pc++)) {

    switch((c)) {
      // skip white spaces
      case ' ': case '\t': case '\r': case '\n': continue;

      // set flags
      case '>': st->little = 0; continue;
      case '<': st->little = 1; continue;
      case '=': st->dont_move = 1; continue;

      case '{':
        {
          if (st->in_tb != 0) luaL_error(st->L, "nested table detected");
          st->in_tb = st->tb_idx = 1;
          st->ret++;
          CHECK_STACK_SPACE(st);
          lua_createtable(st->L, 32, 0);
          continue;
        }
      case '}':
        {
          if (st->in_tb == 0) luaL_error(st->L, "missing corresponding {");
          st->in_tb = 0;
          continue;
        }

      case '$':
      case '&':
        {
          if (*pc != 'u' && *pc != 'i') luaL_error(st->L, "u/i expected after &");

          // 变量栈增长
          int64_t cur_size = st->variable[0];
          if (cur_size - st->index - st->repeat < 0) {
            int grow_size = st->repeat + MEM_GROW_SIZE;
            int64_t * ns = alloca(sizeof(int64_t) * (cur_size + grow_size));
            ns[0] = cur_size + grow_size;
            memmove(ns + 1, st->variable + 1, sizeof(int64_t) * cur_size);
            st->variable = ns;
          }
          if (c == '$') st->create_var = 1; else st->create_ref = 1;
          continue;
        }

      case '+':
        {
          st->pointer += calc_integral_expression(st, &pc, 1);
          int64_t offset = st->pointer - st->buffer;
          if (offset < 0 || (st->buffer_size != 0 && offset > st->buffer_size)) {
            luaL_error(st->L, "+ move out of buffer");
          }
          continue;
        }
      case '-':
        {
          st->pointer -= calc_integral_expression(st, &pc, 1);
          int64_t offset = st->pointer - st->buffer;
          if (offset < 0 || (st->buffer_size != 0 && offset > st->buffer_size)) {
            luaL_error(st->L, "- move out of buffer");
          }
          continue;
        }
      case '*':
        {
          if (st->repeat > 1) luaL_error(st->L, "duplicate repeat flag");
          int64_t repeat_count = calc_integral_expression(st, &pc, 0);
          if (repeat_count <= 0 || (st->in_tb == 0 && repeat_count > 127)) {
            luaL_error(st->L, "invalid repeat times: [%I]", repeat_count);
          }
          st->repeat = (uint32_t)repeat_count;
          continue;
        }

      case '@':
      case '^':
        {
          lua_pushinteger(st->L, st->pointer - st->buffer);
          if (st->in_tb == 0) {
            st->ret++;
            CHECK_STACK_SPACE(st);
          } else {
            lua_rawseti(st->L, -2, st->tb_idx++);
          }
          continue;
        }

      case 'b': read_boolean(st, &pc); break;
      case 'i': case 'u': read_integer(st, &pc, c == 'i'); break;
      case 'f': read_float32(st); break;
      case 'd': read_float64(st); break;
      case 's': read_string(st, &pc); break;
      case 'c': read_fixed_string(st, &pc); break;
      default :
        luaL_error(st->L, "unknown read opcode");
    }

    st->repeat = 1;
  }
}

static int buf_read (lua_State *L) {
  struct State st = {
    .L = L, .repeat = 1, .little = 1,
    .stack_space = INIT_LUA_STACK_SPACE,
    .create_var = 0, .create_ref = 0, .dont_move = 0,
    .ret = 0, .in_tb = 0, .tb_idx = 0,
  };

  if (lua_type(L, 1) == LUA_TNUMBER) {
    st.pointer = st.buffer = (void *)(luaL_checkinteger(L, 1));
    st.buffer_size = 0; // unknown buffer size
  } else {
    st.pointer = st.buffer = luaL_checklstring(L, 1, &st.buffer_size);
    st.buffer_end = st.buffer + st.buffer_size;
  }

  st.instructions = luaL_checkstring(L, 2);

  luaL_checkstack(L, INIT_LUA_STACK_SPACE, NULL);
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

// vim: tabstop=2 softtabstop=2 shiftwidth=2 smarttab expandtab shiftround
