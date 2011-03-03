/*
 * Copyright 2009, 2010 Samy Al Bahra.
 * Copyright 2011 Devon H. O'Dell <devon.odell@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _CK_PR_X86_H
#define _CK_PR_X86_H

#ifndef _CK_PR_H
#error Do not include this file directly, use ck_pr.h
#endif

#include <stdbool.h>
#include <ck_stdint.h>

#include <ck_cc.h>

/*
 * The following represent supported atomic operations.
 * These operations may be emulated.
 */ 
#include "ck_f_pr.h"

/* Minimum requirements for the CK_PR interface are met. */
#define CK_F_PR

#ifdef CK_MD_UMP
#define CK_PR_LOCK_PREFIX
#else
#define CK_PR_LOCK_PREFIX "lock "
#endif

/*
 * Prevent speculative execution in busy-wait loops (P4 <=)
 * or "predefined delay".
 */
CK_CC_INLINE static void
ck_pr_stall(void)
{
	__asm__ __volatile__("pause" ::: "memory");
	return;
}

/*
 * IA32 has strong memory ordering guarantees, so memory
 * fences are enabled if and only if the user specifies that
 * that the program will be using non-temporal instructions.
 * Otherwise, an optimization barrier is used in order to prevent
 * compiler re-ordering of loads and stores across the barrier.
 */
#define CK_PR_FENCE(T, I)				\
	CK_CC_INLINE static void			\
	ck_pr_fence_strict_##T(void)			\
	{						\
		__asm__ __volatile__(I ::: "memory");	\
	}						\
	CK_CC_INLINE static void ck_pr_fence_##T(void)	\
	{						\
		__asm__ __volatile__("" ::: "memory");	\
	}

CK_PR_FENCE(load, "lfence")
CK_PR_FENCE(load_depends, "")
CK_PR_FENCE(store, "sfence")
CK_PR_FENCE(memory, "mfence")

#undef CK_PR_FENCE

/*
 * Atomic fetch-and-store operations.
 */
#define CK_PR_FAS(S, M, T, C, I)				\
	CK_CC_INLINE static T					\
	ck_pr_fas_##S(M *target, T v)				\
	{							\
		__asm__ __volatile__(I " %0, %1"		\
					: "=m" (*(C *)target),	\
					  "+q" (v)		\
					: "m"  (*(C *)target)	\
					: "memory");		\
		return v;					\
	}

CK_PR_FAS(ptr, void, void *, char, "xchgl")

#define CK_PR_FAS_S(S, T, I) CK_PR_FAS(S, T, T, T, I)

CK_PR_FAS_S(char, char, "xchgb")
CK_PR_FAS_S(uint, unsigned int, "xchgl")
CK_PR_FAS_S(int, int, "xchgl")
CK_PR_FAS_S(32, uint32_t, "xchgl")
CK_PR_FAS_S(16, uint16_t, "xchgw")
CK_PR_FAS_S(8,  uint8_t,  "xchgb")

#undef CK_PR_FAS_S
#undef CK_PR_FAS

/*
 * Atomic load-from-memory operations.
 */
CK_CC_INLINE static uint64_t
ck_pr_load_64(uint64_t *target)
{
	uint64_t r;

	__asm__ __volatile__("movq %0, %%xmm0;"
			     "movq %%xmm0, %1;"
				: "+m" (*target),
				  "=m" (r)
				:
				: "memory", "%xmm0");

	return (r);
}

#define CK_PR_LOAD(S, M, T, C, I)				\
	CK_CC_INLINE static T					\
	ck_pr_load_##S(M *target)				\
	{							\
		T r;						\
		__asm__ __volatile__(I " %1, %0"		\
					: "=q" (r)		\
					: "m"  (*(C *)target)	\
					: "memory");		\
		return (r);					\
	}

CK_PR_LOAD(ptr, void, void *, char, "movl")

#define CK_PR_LOAD_S(S, T, I) CK_PR_LOAD(S, T, T, T, I)

CK_PR_LOAD_S(char, char, "movb")
CK_PR_LOAD_S(uint, unsigned int, "movl")
CK_PR_LOAD_S(int, int, "movl")
CK_PR_LOAD_S(32, uint32_t, "movl")
CK_PR_LOAD_S(16, uint16_t, "movw")
CK_PR_LOAD_S(8,  uint8_t,  "movb")

#undef CK_PR_LOAD_S
#undef CK_PR_LOAD

CK_CC_INLINE static void 
ck_pr_load_32_2(uint32_t target[2], uint32_t v[2])
{
#ifdef __PIC__
	uint32_t ebxt;

	__asm__ __volatile__("movl %%ebx, %3;"
			     "movl %%edx, %%ecx;"
			     "movl %%eax, %%ebx;"
			     CK_PR_LOCK_PREFIX "cmpxchg8b %a2;"
			     "movl %3, %%ebx;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)target),
#else
				: "=m" (*(uint32_t *)target),
#endif
				  "=a" (v[0]),
				  "=d" (v[1])
				: "m"  (ebxt)
				: "%ecx", "memory", "cc");
#else
	__asm__ __volatile__("movl %%edx, %%ecx;"
			     "movl %%eax, %%ebx;"
			     CK_PR_LOCK_PREFIX "cmpxchg8b %0;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)target),
#else
				: "=m" (*(uint32_t *)target),
#endif
				  "=a" (v[0]),
				  "=d" (v[1])
				:
				: "%ebx", "%ecx", "memory", "cc");
#endif
	return;
}

CK_CC_INLINE static void
ck_pr_load_ptr_2(void *t, void *v)
{
	ck_pr_load_32_2(t, v);
	return;	
}

#define CK_PR_LOAD_2(S, W, T)					\
	CK_CC_INLINE static void				\
	ck_pr_load_##S##_##W(T t[2], T v[2])			\
	{							\
		ck_pr_load_32_2((uint32_t *)t, (uint32_t *)v);	\
		return;						\
	}

CK_PR_LOAD_2(char, 8, char)
CK_PR_LOAD_2(int, 2, int)
CK_PR_LOAD_2(uint, 2, unsigned int)
CK_PR_LOAD_2(16, 4, uint16_t)
CK_PR_LOAD_2(8, 8, uint8_t)

#undef CK_PR_LOAD_2

/*
 * Atomic store-to-memory operations.
 */
CK_CC_INLINE static void
ck_pr_store_64(uint64_t *target, uint64_t val)
{
	__asm__ __volatile__("movq %1, %0;"
				: "+m" (*target)
				: "y" (val)
				: "memory");
}

#define CK_PR_STORE(S, M, T, C, I)				\
	CK_CC_INLINE static void				\
	ck_pr_store_##S(M *target, T v)				\
	{							\
		__asm__ __volatile__(I " %1, %0"		\
					: "=m" (*(C *)target)	\
					: "iq" (v)		\
					: "memory");		\
		return;						\
	}

CK_PR_STORE(ptr, void, void *, char, "movl")

#define CK_PR_STORE_S(S, T, I) CK_PR_STORE(S, T, T, T, I)

CK_PR_STORE_S(char, char, "movb")
CK_PR_STORE_S(uint, unsigned int, "movl")
CK_PR_STORE_S(int, int, "movl")
CK_PR_STORE_S(32, uint32_t, "movl")
CK_PR_STORE_S(16, uint16_t, "movw")
CK_PR_STORE_S(8,  uint8_t, "movb")

#undef CK_PR_STORE_S
#undef CK_PR_STORE

/*
 * Atomic fetch-and-add operations.
 */
#define CK_PR_FAA(S, M, T, C, I)					\
	CK_CC_INLINE static T						\
	ck_pr_faa_##S(M *target, T d)					\
	{								\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX I " %1, %0"	\
					: "+m" (*(C *)target),		\
					  "+q" (d)			\
					:				\
					: "memory", "cc");		\
		return (d);						\
	}

CK_PR_FAA(ptr, void, uintptr_t, char, "xaddl")

#define CK_PR_FAA_S(S, T, I) CK_PR_FAA(S, T, T, T, I)

CK_PR_FAA_S(char, char, "xaddb")
CK_PR_FAA_S(uint, unsigned int, "xaddl")
CK_PR_FAA_S(int, int, "xaddl")
CK_PR_FAA_S(32, uint32_t, "xaddl")
CK_PR_FAA_S(16, uint16_t, "xaddw")
CK_PR_FAA_S(8,  uint8_t,  "xaddb")

#undef CK_PR_FAA_S
#undef CK_PR_FAA

/*
 * Atomic store-only unary operations.
 */
#define CK_PR_UNARY(K, S, T, C, I)				\
	CK_PR_UNARY_R(K, S, T, C, I)				\
	CK_PR_UNARY_V(K, S, T, C, I)

#define CK_PR_UNARY_R(K, S, T, C, I)				\
	CK_CC_INLINE static void				\
	ck_pr_##K##_##S(T *target)				\
	{							\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX I " %0"	\
					: "+m" (*(C *)target)	\
					:			\
					: "memory", "cc");	\
		return;						\
	}

#define CK_PR_UNARY_V(K, S, T, C, I)					\
	CK_CC_INLINE static void					\
	ck_pr_##K##_##S##_zero(T *target, bool *r)			\
	{								\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX I " %0; setz %1"	\
					: "+m" (*(C *)target),		\
					  "=m" (*r)			\
					:				\
					: "memory", "cc");		\
		return;							\
	}


#define CK_PR_UNARY_S(K, S, T, I) CK_PR_UNARY(K, S, T, T, I)

#define CK_PR_GENERATE(K)				\
	CK_PR_UNARY(K, ptr, void, char, #K "l") 	\
	CK_PR_UNARY_S(K, char, char, #K "b")		\
	CK_PR_UNARY_S(K, int, int, #K "l")		\
	CK_PR_UNARY_S(K, uint, unsigned int, #K "l")	\
	CK_PR_UNARY_S(K, 32, uint32_t, #K "l")		\
	CK_PR_UNARY_S(K, 16, uint16_t, #K "w")		\
	CK_PR_UNARY_S(K, 8, uint8_t, #K "b")

CK_PR_GENERATE(inc)
CK_PR_GENERATE(dec)
CK_PR_GENERATE(neg)

/* not does not affect condition flags. */
#undef CK_PR_UNARY_V
#define CK_PR_UNARY_V(a, b, c, d, e)
CK_PR_GENERATE(not)

#undef CK_PR_GENERATE
#undef CK_PR_UNARY_S
#undef CK_PR_UNARY_V
#undef CK_PR_UNARY_R
#undef CK_PR_UNARY

/*
 * Atomic store-only binary operations.
 */
#define CK_PR_BINARY(K, S, M, T, C, I)					\
	CK_CC_INLINE static void					\
	ck_pr_##K##_##S(M *target, T d)					\
	{								\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX I " %1, %0"	\
					: "+m" (*(C *)target)		\
					: "iq" (d)			\
					: "memory", "cc");		\
		return;							\
	}

#define CK_PR_BINARY_S(K, S, T, I) CK_PR_BINARY(K, S, T, T, T, I)

#define CK_PR_GENERATE(K)					\
	CK_PR_BINARY(K, ptr, void, uintptr_t, char, #K "l")	\
	CK_PR_BINARY_S(K, char, char, #K "b")			\
	CK_PR_BINARY_S(K, int, int, #K "l")			\
	CK_PR_BINARY_S(K, uint, unsigned int, #K "l")		\
	CK_PR_BINARY_S(K, 32, uint32_t, #K "l")			\
	CK_PR_BINARY_S(K, 16, uint16_t, #K "w")			\
	CK_PR_BINARY_S(K, 8, uint8_t, #K "b")

CK_PR_GENERATE(add)
CK_PR_GENERATE(sub)
CK_PR_GENERATE(and)
CK_PR_GENERATE(or)
CK_PR_GENERATE(xor)

#undef CK_PR_GENERATE
#undef CK_PR_BINARY_S
#undef CK_PR_BINARY

/*
 * Atomic compare and swap.
 */
#define CK_PR_CAS(S, M, T, C, I)						\
	CK_CC_INLINE static bool						\
	ck_pr_cas_##S(M *target, T compare, T set)				\
	{									\
		bool z;								\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX I " %2, %0; setz %1"	\
					: "+m"  (*(C *)target),			\
					  "=a"  (z)				\
					: "q"   (set),				\
					  "a"   (compare)			\
					: "memory", "cc");			\
		return z;							\
	}

CK_PR_CAS(ptr, void, void *, char, "cmpxchgl")

#define CK_PR_CAS_S(S, T, I) CK_PR_CAS(S, T, T, T, I)

CK_PR_CAS_S(char, char, "cmpxchgb")
CK_PR_CAS_S(int, int, "cmpxchgl")
CK_PR_CAS_S(uint, unsigned int, "cmpxchgl")
CK_PR_CAS_S(32, uint32_t, "cmpxchgl")
CK_PR_CAS_S(16, uint16_t, "cmpxchgw")
CK_PR_CAS_S(8,  uint8_t,  "cmpxchgb")

#undef CK_PR_CAS_S
#undef CK_PR_CAS

/*
 * Compare and swap, set *v to old value of target.
 */
#define CK_PR_CAS_O(S, M, T, C, I, R)						\
	CK_CC_INLINE static bool						\
	ck_pr_cas_##S##_value(M *target, T compare, T set, M *v)		\
	{									\
		bool z;								\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX "cmpxchg" I " %3, %0;"	\
				     "mov %% " R ", %2;"			\
				     "setz %1;"					\
					: "+m"  (*(C *)target),			\
					  "=a"  (z),				\
					  "=m"  (*(C *)v)			\
					: "q"   (set),				\
					  "a"   (compare)			\
					: "memory", "cc");			\
		return (bool)z;							\
	}

CK_PR_CAS_O(ptr, void, void *, char, "l", "eax")

#define CK_PR_CAS_O_S(S, T, I, R)	\
	CK_PR_CAS_O(S, T, T, T, I, R)

CK_PR_CAS_O_S(char, char, "b", "al")
CK_PR_CAS_O_S(int, int, "l", "eax")
CK_PR_CAS_O_S(uint, unsigned int, "l", "eax")
CK_PR_CAS_O_S(32, uint32_t, "l", "eax")
CK_PR_CAS_O_S(16, uint16_t, "w", "ax")
CK_PR_CAS_O_S(8,  uint8_t,  "b", "al")

#undef CK_PR_CAS_O_S
#undef CK_PR_CAS_O

CK_CC_INLINE static bool
ck_pr_cas_64(uint64_t *t, uint64_t c, uint64_t s)
{
	bool z;

	union {
		uint64_t s;
		uint32_t v[2];
	} set;

	uint64_t A;

#ifdef __PIC__
	uint32_t ebxt;
#endif


	ck_pr_store_64(&set.s, s);

#ifdef __PIC__
	__asm__ __volatile__("movl %%ebx, %6;"
			     "movl %5, %%ebx;"
			     CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %2;"
			     "movl %6, %%ebx;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)t),
#else
				: "=m" (*(uint32_t *)t),
#endif
				  "=A" (A),
				  "=q" (z)
				: "A"  (c),
				  "c"  (set.v[1]),
				  "m"  (set.v[0]),
				  "m"  (ebxt)
				: "memory", "cc");
#else
	__asm__ __volatile__(CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %2;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)t),
#else
				: "=m" (*(uint32_t *)t),
#endif
				  "=A" (A),
				  "=q" (z)
				: "A"  (c),
				  "b"  (set.v[0]),
				  "c"  (set.v[1])
				: "memory", "cc");
#endif
	return (bool)z;
}

CK_CC_INLINE static bool
ck_pr_cas_64_value(uint64_t *t, uint64_t c, uint64_t s, uint64_t *v)
{
	bool z;
	union {
		uint64_t s;
		uint32_t v[2];
	} set;
	uint32_t *val = (uint32_t *)v;

#ifdef __PIC__
	uint32_t ebxt;
#endif

	ck_pr_store_64(&set.s, s);

#ifdef __PIC__
	__asm__ __volatile__("movl %%ebx, %7;"
			     "movl %6, %%ebx;"
			     CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %3;"
			     "movl %7, %%ebx;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)t),
#else
				: "=m" (*(uint32_t *)t),
#endif
				  "=a" (val[0]),
				  "=d" (val[1]),
				  "=q" (z)
				: "A"  (c),
				  "c"  (set.v[1]),
				  "m"  (set.v[0]),
				  "m"  (ebxt)
				: "memory", "cc");
#else
	__asm__ __volatile__(CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %3;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)t),
#else
				: "=m" (*(uint32_t *)t),
#endif
				  "=a" (val[0]),
				  "=d" (val[1]),
				  "=q" (z)
				: "A"  (c),
				  "b"  (set.v[0]),
				  "c"  (set.v[1])
				: "memory", "cc");

#endif
	return (bool)z;
}

CK_CC_INLINE static bool
ck_pr_cas_32_2(uint32_t t[2], uint32_t c[2], uint32_t s[2])
{
	bool z;

	uint64_t A;

#ifdef __PIC__
	uint32_t ebxt;

	__asm__ __volatile__("movl %%ebx, %6;"
			     "movl %5, %%ebx;"
			     CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %2;"
			     "movl %6, %%ebx;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)t),
#else
				: "=m" (*(uint32_t *)t),
#endif
				  "=A" (A),
				  "=q" (z)
				: "A"  (c),
				  "c"  (s[1]),
				  "m"  (s[0]),
				  "m"  (ebxt)
				: "memory", "cc");
#else
	__asm__ __volatile__(CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %2;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)t),
#else
				: "=m" (*(uint32_t *)t),
#endif
				  "=A" (A),
				  "=q" (z)
				: "a"  (c[0]),
				  "d"  (c[1]),
				  "b"  (s[0]),
				  "c"  (s[1])
				: "memory", "cc");
#endif

	return (bool)z;
}

CK_CC_INLINE static bool
ck_pr_cas_ptr_2(void *t, void *c, void *s)
{
	return ck_pr_cas_32_2(t, c, s);
}


CK_CC_INLINE static bool
ck_pr_cas_32_2_value(uint32_t target[2], uint32_t compare[2], uint32_t set[2], uint32_t v[2])
{
	bool z;

#ifdef __PIC__
	uint32_t ebxt;

	__asm__ __volatile__("movl %%ebx, %7;"
			     "movl %6, %%ebx;"
			     CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %3;"
			     "movl %7, %%ebx;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)target),
#else
				: "=m" (*(uint32_t *)target),
#endif
				  "=a" (v[0]),
				  "=d" (v[1]),
				  "=q" (z)
				: "A"  (compare),
				  "c"  (set[1]),
				  "m"  (set[0]),
				  "m"  (ebxt)
				: "memory", "cc");
#else
	__asm__ __volatile__(CK_PR_LOCK_PREFIX "cmpxchg8b %0; setz %3;"
#if (__GNUC__ * 100 + __GNUC_MINOR__) >= 403
				: "+m" (*(uint32_t *)target),
#else
				: "=m" (*(uint32_t *)target),
#endif
				  "=a" (v[0]),
				  "=d" (v[1]),
				  "=q" (z)
				: "A" (compare),
				  "b" (set[0]),
				  "c" (set[1])
				: "memory", "cc");
#endif
	return (bool)z;
}

CK_CC_INLINE static bool
ck_pr_cas_ptr_2_value(void *t, void *c, void *s, void *v)
{
	return ck_pr_cas_32_2_value(t, c, s, v);
}

#define CK_PR_CAS_V(S, W, T)				\
CK_CC_INLINE static bool				\
ck_pr_cas_##S##_##W(T t[W], T c[W], T s[W])		\
{							\
	return ck_pr_cas_32_2((uint32_t *)t,		\
			      (uint32_t *)c,		\
			      (uint32_t *)s);		\
}							\
CK_CC_INLINE static bool				\
ck_pr_cas_##S##_##W##_value(T *t, T c[W], T s[W], T *v)	\
{							\
	return ck_pr_cas_32_2_value((uint32_t *)t,	\
				    (uint32_t *)c,	\
				    (uint32_t *)s,	\
				    (uint32_t *)v);	\
}

CK_PR_CAS_V(char, 8, char)
CK_PR_CAS_V(int, 2, int)
CK_PR_CAS_V(uint, 2, unsigned int)
CK_PR_CAS_V(16, 4, uint16_t)
CK_PR_CAS_V(8, 8, uint8_t)

#undef CK_PR_CAS_V

/*
 * Atomic bit test operations.
 */
#define CK_PR_BT(K, S, T, P, C, I)					\
	CK_CC_INLINE static bool					\
	ck_pr_##K##_##S(T *target, unsigned int b)			\
	{								\
		bool c;							\
		__asm__ __volatile__(CK_PR_LOCK_PREFIX I "; setc %1"	\
					: "+m" (*(C *)target),		\
					  "=q" (c)			\
					: "q"  ((P)b)			\
					: "memory", "cc");		\
		return (bool)c;						\
	}

#define CK_PR_BT_S(K, S, T, I) CK_PR_BT(K, S, T, T, T, I)

#define CK_PR_GENERATE(K)					\
	CK_PR_BT(K, ptr, void, uint32_t, char, #K "l %2, %0")	\
	CK_PR_BT_S(K, uint, unsigned int, #K "l %2, %0")	\
	CK_PR_BT_S(K, int, int, #K "l %2, %0")			\
	CK_PR_BT_S(K, 32, uint32_t, #K "l %2, %0")		\
	CK_PR_BT_S(K, 16, uint16_t, #K "w %w2, %0")

/* TODO: GCC's intrinsic atomics for btc and bts don't work for 64-bit. */

CK_PR_GENERATE(btc)
CK_PR_GENERATE(bts)
CK_PR_GENERATE(btr)

#undef CK_PR_GENERATE
#undef CK_PR_BT

#endif /* _CK_PR_X86_H */