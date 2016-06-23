#include "buflib.h"

#define MAX_SIZET ((size_t)(~(size_t)0))
#define MAXSIZE   (sizeof(size_t) < sizeof(int) ? MAX_SIZET : (size_t)(INT_MAX))

#define NB  CHAR_BIT

//mask for one character (NB 1's)

#define MC	((1 << NB) - 1)
// size of a lua_Integer
#define SIZE_LUA_INTEGER   ((int)sizeof(lua_Integer))

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
getnum_with_limit (struct State *st, const char **s,
    uint32_t df, uint32_t min, uint32_t max) {
  uint32_t num = getnum(s, df);
  if (max < num || num < min)
    luaL_error(st->L, "(%u) out of limits [%u, %u]", num, min, max);
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


enum kOperation next_operation(struct State * st) {
  enum kOperation k_ret = k_hlt;

  const char * pc = st->instructions;
  char c;

start:
  switch((c = *pc++)) {
    case ' ': case '\t': case '\r': case '\n': goto start;
    case '>': st->is_little = 0; goto start;
    case '<': st->is_little = 1; goto start;
    case '=': st->is_little = 1; goto start; // TODO native endian check
    case '$':
    case '@':
    case '%':
    case '(':
    case ')':
    case '+': st->pointer = st->pointer + getnum(&pc, 1); goto start;
    case '-': st->pointer = st->pointer - getnum(&pc, 1); goto start;
    case '*':
    case 'b':
    case 'i':
    case 'u': {
      uint32_t size = getnum_with_limit(st, &pc, 4, 1, 8);
      st->ivalue = unpackint(st, st->pointer, st->is_little,
          size, 0);
      st->pointer += size;
      k_ret = k_int;
      break;
              }
    case 'f':
    case 'd':
    case 's':
      break;
  }

  st->instructions = pc;

  return k_ret;
}

#define INIT_MEM_SIZE 32

static int buf_read (lua_State *L) {
  struct State st = {
    .index = 0, .offset = 0, .repeat = 0, .is_eval = 0,
    .ret = 0, .L = L, .is_little = 1,
  };

  st.pointer = st.buffer = luaL_checklstring(L, 1, &st.buffer_size);
  st.instructions = luaL_checkstring(L, 2);
  st.variable = alloca(sizeof(int64_t) * INIT_MEM_SIZE);
  if (!st.variable) {
    return luaL_error(L, "malloc failed");
  }
  st.variable[0] = INIT_MEM_SIZE;

  //'< u4 +12 i4'

  enum kOperation op;

  luaL_checkstack(L, 32, "no more stack to push result"); // TODO

  while ((op = next_operation(&st)) != k_hlt) {
    switch (op) {
      case k_bool:
        lua_pushboolean(L, (int)st.ivalue);
        st.ret++;
        break;
      case k_int:
        lua_pushinteger(L, (lua_Integer)st.ivalue);
        st.ret++;
        break;
      case k_float:
        lua_pushnumber(L, st.fvalue);
        st.ret++;
        break;
      case k_string:
        lua_pushstring(L, st.svalue);
        st.ret++;
        break;
      case k_lstring:
        lua_pushlstring(L, st.svalue, st.len);
        st.ret++;
        break;
    }
  }

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
