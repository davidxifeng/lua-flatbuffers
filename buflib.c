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


// Read an integer numeral and raises an error if it is larger
// than the maximum size for integers.
static uint32_t
get_opt_int_size (struct State *st, const char **s,
    uint32_t df, uint32_t min, uint32_t max) {
  uint32_t num = getnum(s, df);
  if (max < num || num < min)
    luaL_error(st->L, "(%u) out of limits [%u, %u]", num, min, max);
  switch (num) {
    case 0: case 1: case 2: case 4: case 8: break;
    default: luaL_error(st->L, "invalid integer size (%u)", num);
  }
  return num;
}

// TODO 依赖 lua_Integer 64位

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
#define INIT_STACK_SPACE 32
#define STACK_GROW_SPACE 16

static void check_stack_space(struct State *st) {
  if (st->ret >= st->stack_space) {
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

static void read_boolean(struct State * st, const char **s) {
  uint32_t sz = get_opt_int_size(st, s, 1, 1, 8);

  while (st->repeat-- > 0) {
    check_stack_space(st);
    lua_pushboolean(st->L, unpackint(st, st->pointer, st->little, sz, 0));
    st->pointer += sz;
    st->ret++;
  }
}

static void read_integer(struct State *st, const char **s, int is_sign) {
  uint32_t sz = get_opt_int_size(st, s, 4, 1, 8);

  while (st->repeat-- > 0) {
    check_stack_space(st);
    lua_pushinteger(st->L, unpackint(st, st->pointer, st->little, sz, is_sign));
    st->pointer += sz;
    st->ret++;
  }
}

static void read_float(struct State *st, int size) {
  while (st->repeat-- > 0) {
    check_stack_space(st);
    volatile union Ftypes u;
    lua_Number num;
    copy_with_endian(u.buff, st->pointer, size, st->little);
    if (size == sizeof(u.f)) num = (lua_Number)u.f;
    else if (size == sizeof(u.d)) num = (lua_Number)u.d;
    else num = u.n;
    lua_pushnumber(st->L, num);
    st->pointer += size;
    st->ret++;
  }
}

static void read_string(struct State *st, const char **s) {
  uint32_t sz = get_opt_int_size(st, s, 0, 1, 4);

  while (st->repeat -- > 0) {
    check_stack_space(st);
    if (sz == 0) {
      size_t slen = strlen(st->pointer);
      lua_pushlstring(st->L, st->pointer, slen);
      st->pointer += slen + 1; // skip a terminal zero
    } else {
      uint32_t slen = unpackint(st, st->pointer, st->little, sz, 0);
      st->pointer += sz;
      lua_pushlstring(st->L, st->pointer, slen);
      st->pointer += slen;
    }
    st->ret++;
  }
}

static void read_fixed_string(struct State *st, const char **s) {
  uint32_t sz = getnum(s, 0);
  if (sz == 0) luaL_error(st->L, "bad n in 'c[n]' format");

  while (st->repeat-- > 0) {
    check_stack_space(st);
    lua_pushlstring(st->L, st->pointer, sz);
    st->pointer += sz;
    st->ret++;
  }
}

static void run_instructions(struct State * st) {
  const char * pc = st->instructions;
  char c;

  while ((c = *pc++)) {

    switch((c)) {
      case ' ': case '\t': case '\r': case '\n': goto next_loop;
      case '>': st->little = 0; goto next_loop;
      case '<': st->little = 1; goto next_loop;
      case '=': st->little = 1; goto next_loop; // TODO native endian check
      case '$':
      case '&':
      case '%':
      case '+': st->pointer = st->pointer + getnum(&pc, 1); goto next_loop;
      case '-': st->pointer = st->pointer - getnum(&pc, 1); goto next_loop;
      case '*':
        {
        if (st->repeat > 1) luaL_error(st->L, "duplicate repeat flag");
        st->repeat = getnum(&pc, 0);
        if (st->repeat == 0) luaL_error(st->L, "bad repeat times in '*n'");
        goto next_loop;
        }

      case 'b': read_boolean(st, &pc); goto reset_repeat;
      case 'i': case 'u': read_integer(st, &pc, c == 'i'); goto reset_repeat;
      case 'f': case 'd': read_float(st, c == 'f' ? 4 : 8); goto reset_repeat;
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

static int buf_read (lua_State *L) {
  struct State st = {
    .index = 0, .offset = 0, .repeat = 1, .is_eval = 0,
    .ret = 0, .L = L, .little = 1, .stack_space = INIT_STACK_SPACE,
  };

  st.pointer = st.buffer = luaL_checklstring(L, 1, &st.buffer_size);
  st.instructions = luaL_checkstring(L, 2);
  st.variable = alloca(sizeof(int64_t) * INIT_MEM_SIZE);
  if (!st.variable) {
    return luaL_error(L, "malloc failed");
  }
  st.variable[0] = INIT_MEM_SIZE;
  luaL_checkstack(L, INIT_STACK_SPACE, NULL);
  run_instructions(&st);
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
