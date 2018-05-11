
#ifndef _OFBTYPES_H_
#define _OFBTYPES_H_

// OFBTypes.h
//
// Sergei <Neill3d> Solokhin (https://github.com/Neill3d/OpenFBX)
//

#ifndef __STDC_LIMIT_MACROS		// need to be defined to have access to UINTPTR_MAX on Linux
#define __STDC_LIMIT_MACROS
#endif

#include <float.h>
#include <stddef.h>
#include <stdint.h>

namespace ofbx
{


	//
	struct CStaticName
	{
		char    raw[32];
	};

	typedef unsigned char u8;
	typedef unsigned short u16;
	typedef unsigned int u32;
	typedef unsigned long long u64;
	typedef signed long long i64;

	static_assert(sizeof(u8) == 1, "u8 is not 1 byte");
	static_assert(sizeof(u32) == 4, "u32 is not 4 bytes");
	static_assert(sizeof(u64) == 8, "u64 is not 8 bytes");
	static_assert(sizeof(i64) == 8, "i64 is not 8 bytes");

	struct OFBVector2
	{
		double x;
		double y;
	};

	struct OFBVector3
	{
		double x;
		double y;
		double z;

		double &operator [] (int index)
		{
			return *(&x + index);
		}
		double operator [] (int index) const
		{
			return *(&x + index);
		}
	};

	struct OFBVector4
	{
		double x;
		double y;
		double z;
		double w;

		double &operator [] (int index)
		{
			return *(&x + index);
		}
		double operator [] (int index) const
		{
			return *(&x + index);
		}
	};

	struct OFBColor
	{
		double r;
		double g;
		double b;
	};

	struct OFBMatrix
	{
		double m[16];

		double &operator [] (int index)
		{
			return m[index];
		}
		double operator [] (int index) const
		{
			return m[index];
		}
		
		double &operator () (int col, int row)
		{
			return m[col * 4 + row];
		}
		double operator () (int col, int row) const
		{
			return m[col * 4 + row];
		}
	};

	// Common independant definitions

	typedef signed char			kByte;         /* signed 8-bit integer         */
	typedef unsigned char		kUByte;        /* unsigned 8-bit integer       */
	typedef signed char			kSChar;
	typedef unsigned char		kUChar;
	typedef unsigned int		kUInt;
	typedef signed short		kShort;
	typedef unsigned short		kUShort;

	typedef double				kVector2[2];   /* 2 x 64-bit real              */
	typedef double				kVector3[3];   /* 3 x 64-bit real              */
	typedef double				kVector4[4];   /* 4 x 64-bit real              */
	typedef double				kMatrix[16];

#define K_SCHAR_MIN			(-128)
#define K_SCHAR_MAX			(127)
#define K_UCHAR_MIN			(0)
#define K_UCHAR_MAX			(255)

#define K_INT8_MIN			(-128)
#define K_INT8_MAX			(127)
#define K_UINT8_MIN			(0)
#define K_UINT8_MAX			(255)

#define K_INT16_MIN			(-32768)
#define K_INT16_MAX			(32767)
#define K_UINT16_MIN		(0)
#define K_UINT16_MAX		(65535)

#define K_INT32_MIN			(0x80000000)
#define K_INT32_MAX			(0x7fffffff)
#define K_UINT32_MIN		(0)
#define K_UINT32_MAX		(0xffffffff)

#if defined (KARCH_ARCH_POWERPC)
#define KARCH_BIG_ENDIAN
#else
#define KARCH_LITTLE_ENDIAN
#endif

#define K_CHAR(x)			char(x)
#define K_SCHAR(x)			kSChar(x)
#define K_UCHAR(x)			kUChar(x##L)

#ifndef _T
#define __T(x)      x
#define _T(x)       __T(x)
#endif

#define K_SHORT(x)			kShort(x)
#define K_USHORT(x)			kUShort(x##U)
#define K_SHORT_MIN			K_SHORT(-32768)
#define K_SHORT_MAX			K_SHORT(32767)
#define K_USHORT_MIN		K_USHORT(0)
#define K_USHORT_MAX		K_USHORT(65535)

#define K_INT(x)			(x)
#define K_UINT(x)			(x##U)
#define K_INT_MIN			K_INT(0x80000000)
#define K_INT_MAX			K_INT(0x7fffffff)
#define K_UINT_MIN			K_UINT(0)
#define K_UINT_MAX			K_UINT(0xffffffff)

#ifdef __LP64__
	typedef signed int			kLong;
	typedef unsigned int		kULong;
#else
	typedef signed long			kLong;
	typedef unsigned long		kULong;
#endif

#define K_LONG(x)			(x)
#define K_ULONG(x)			(x##U)
#define K_LONG_MIN			K_LONG(0x80000000)
#define K_LONG_MAX			K_LONG(0x7fffffff)
#define K_ULONG_MIN			K_ULONG(0)
#define K_ULONG_MAX			K_ULONG(0xffffffff)

	typedef signed long long	kLongLong;
#define K_LONGLONG(x)		(x##LL)
	typedef unsigned long long	kULongLong;
#define K_ULONGLONG(x)		(x##ULL)

#define K_LONGLONG_MIN		K_LONGLONG(0x8000000000000000)
#define K_LONGLONG_MAX		K_LONGLONG(0x7fffffffffffffff)
#define K_ULONGLONG_MIN		K_ULONGLONG(0)
#define K_ULONGLONG_MAX		K_ULONGLONG(0xffffffffffffffff)

#define K_FLOAT(x)			(x##F)
#define K_FLOAT_MIN			FLT_MIN					// minimum positive value
#define K_FLOAT_MAX			FLT_MAX					// maximum positive value

#define K_DOUBLE(x)			(x)
#define K_DOUBLE_MIN		DBL_MIN					// minimum positive value
#define K_DOUBLE_MAX		DBL_MAX					// maximum positive value

	typedef kSChar				kInt8;
	typedef kUChar				kUInt8;

	typedef kShort				kInt16;
	typedef kUShort				kUInt16;

	typedef int					kInt32;
	typedef kUInt				kUInt32;

	typedef kLongLong			kInt64;
	typedef kULongLong			kUInt64;
#define K_INT64_MIN			K_LONGLONG_MIN
#define K_INT64_MAX			K_LONGLONG_MAX

#ifdef KARCH_DEV_MSC
#define K_TEMPLATE_SPEC(t)
#define K_TEMPLATE_SPEC2(t1, t2)
#define K_INLINE			inline
#ifdef _DEBUG
#define K_FORCEINLINE		K_INLINE
#else
#define K_FORCEINLINE		__forceinline
#endif
#define K_TLS	__declspec( thread )	//	Thread-Local Storage
#else
#define K_TEMPLATE_SPEC(t)	template<>
#define K_TEMPLATE_SPEC2(t1, t2)
#define K_INLINE			inline
#define K_FORCEINLINE		K_INLINE
#define K_TLS	__thread				//	Thread-Local Storage
#endif

	// Atomic definitions
#ifdef KARCH_ARCH_64
#ifdef KARCH_DEV_MSC
	typedef __int64			kAtomic;
#else
	typedef long long		kAtomic;
#endif
#else
	typedef long				kAtomic;
#endif

	// Pointer definitions
	typedef uintptr_t			kReference;
#define K_REFERENCE_MAX		UINTPTR_MAX

	typedef kULongLong			KMicroTime;

}; 

#endif