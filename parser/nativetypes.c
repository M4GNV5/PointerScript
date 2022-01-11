#include <assert.h>
#include <string.h>

#include "./common.h"
#include "../jit/jit.h"
#include "../jit/include/util.h"

#define define_nativetype(ptrsName, cName, ptrsType, handlerName) \
	{ \
		sizeof(cName), ptrsName, NULL, ptrsType, \
		ptrs_handle_native_get ## handlerName, ptrs_handle_native_set ## handlerName, \
	}

ptrs_nativetype_info_t ptrs_nativeTypes[] = {
	define_nativetype("char",      signed char,        PTRS_TYPE_INT, Int),
	define_nativetype("short",     short,              PTRS_TYPE_INT, Int),
	define_nativetype("int",       int,                PTRS_TYPE_INT, Int),
	define_nativetype("long",      long,               PTRS_TYPE_INT, Int),
	define_nativetype("longlong",  long long,          PTRS_TYPE_INT, Int),
	define_nativetype("uchar",     unsigned char,      PTRS_TYPE_INT, UInt),
	define_nativetype("ushort",    unsigned short,     PTRS_TYPE_INT, UInt),
	define_nativetype("uint",      unsigned int,       PTRS_TYPE_INT, UInt),
	define_nativetype("ulong",     unsigned long,      PTRS_TYPE_INT, UInt),
	define_nativetype("ulonglong", unsigned long long, PTRS_TYPE_INT, UInt),
	define_nativetype("i8",        int8_t,             PTRS_TYPE_INT, Int),
	define_nativetype("i16",       int16_t,            PTRS_TYPE_INT, Int),
	define_nativetype("i32",       int32_t,            PTRS_TYPE_INT, Int),
	define_nativetype("i64",       int64_t,            PTRS_TYPE_INT, Int),
	define_nativetype("u8",        uint8_t,            PTRS_TYPE_INT, UInt),
	define_nativetype("u16",       uint16_t,           PTRS_TYPE_INT, UInt),
	define_nativetype("u32",       uint32_t,           PTRS_TYPE_INT, UInt),
	define_nativetype("u64",       uint64_t,           PTRS_TYPE_INT, UInt),
	define_nativetype("float",     float,              PTRS_TYPE_FLOAT, Float),
	define_nativetype("double",    double,             PTRS_TYPE_FLOAT, Float),
	define_nativetype("f32",       float,              PTRS_TYPE_FLOAT, Float),
	define_nativetype("f64",       double,             PTRS_TYPE_FLOAT, Float),
	define_nativetype("cfunc",     void *,             PTRS_TYPE_POINTER, Pointer),
	define_nativetype("pointer",   void *,             PTRS_TYPE_POINTER, Pointer),
	define_nativetype("bool",      bool,               PTRS_TYPE_INT, UInt),
	define_nativetype("ssize",     ssize_t,            PTRS_TYPE_INT, Int),
	define_nativetype("size",      size_t,             PTRS_TYPE_INT, UInt),
	define_nativetype("intptr",    uintptr_t,          PTRS_TYPE_INT, Int),
	define_nativetype("uintptr",   intptr_t,           PTRS_TYPE_INT, UInt),
	define_nativetype("ptrdiff",   ptrdiff_t,          PTRS_TYPE_INT, Int),
	define_nativetype("var",       ptrs_var_t,         (uint8_t)-1, Var),
};

const int ptrs_nativeTypeCount = sizeof(ptrs_nativeTypes) / sizeof(ptrs_nativetype_info_t);

void ptrs_initialize_nativeTypes()
{
	jit_type_t types[] = {
		jit_type_sys_char,
		jit_type_sys_short,
		jit_type_sys_int,
		jit_type_sys_long,
		jit_type_sys_longlong,

		jit_type_sys_uchar,
		jit_type_sys_ushort,
		jit_type_sys_uint,
		jit_type_sys_ulong,
		jit_type_sys_ulonglong,

		jit_type_sbyte,
		jit_type_short,
		jit_type_int,
		jit_type_long,

		jit_type_ubyte,
		jit_type_ushort,
		jit_type_uint,
		jit_type_ulong,

		jit_type_float32,
		jit_type_float64,
		jit_type_float32,
		jit_type_float64,

		jit_type_void_ptr,
		jit_type_void_ptr,

		jit_type_sys_bool,
		jit_type_nint, //ssize
		jit_type_nuint, //size
		jit_type_nint,
		jit_type_nuint,
		jit_type_nint, //ptrdiff

		ptrs_jit_getVarType(), //var
	};

	// sanity checks
	assert(sizeof(types) / sizeof(jit_type_t) == ptrs_nativeTypeCount);
	assert(sizeof(ptrs_nativeTypes) / sizeof(ptrs_nativetype_info_t) == ptrs_nativeTypeCount);
	assert(strcmp(ptrs_nativeTypes[PTRS_NATIVETYPE_INDEX_CHAR].name, "char") == 0);
	assert(strcmp(ptrs_nativeTypes[PTRS_NATIVETYPE_INDEX_VAR].name, "var") == 0);

	for(int i = 0; i < ptrs_nativeTypeCount; i++)
		ptrs_nativeTypes[i].jitType = types[i];
}
