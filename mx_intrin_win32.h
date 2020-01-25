#pragma once

#include <intrin.h>
#include <stdint.h>

// TODO: This doesn't belong here
#define mx_assert(cond) do { if (!(cond)) __debugbreak(); } while (0)
#define mx_forceinline __forceinline
#define mx_threadlocal __declspec(thread)

#define mx_yield() _mm_pause()

#if defined(_M_X64)

#define mxa_load64(src) (*(const volatile uint64_t*)(src))

mx_forceinline int mxa_cas64(uint64_t *dst, uint64_t cmp, uint64_t val)
{
	return (uint64_t)_InterlockedCompareExchange64((volatile long long*)dst, (long long)val, (long long)cmp) == cmp;
}

mx_forceinline int mxa_double_cas(void *dst, const void *cmp, const void *val)
{
	const long long *lcmp = (const long long*)cmp, *lval = (const long long)val;
	long long mut_cmp[2] = { lcmp[0], lcmp[1] };
	return _InterlockedCompareExchange128(dst, lval[1], lval[0], mut_cmp);
}

#elif defined(_M_IX86)

#define mxa_load32(src) (*(const volatile uint32_t*)(src))
#define mxa_inc32(dst) ((uint32_t)_InterlockedIncrement((volatile long*)(dst)) - 1)
#define mxa_dec32(dst) ((uint32_t)_InterlockedDecrement((volatile long*)(dst)) + 1)
#define mxa_add32(dst, val) ((uint32_t)_InterlockedExchangeAdd((volatile long*)(dst), (long)val))
#define mxa_sub32(dst, val) ((uint32_t)_InterlockedExchangeAdd((volatile long*)(dst), -(long)val))

mx_forceinline int mxa_cas32(uint32_t *dst, uint32_t cmp, uint32_t val)
{
	return (uint32_t)_InterlockedCompareExchange((volatile long*)dst, (long)val, (long)cmp) == cmp;
}

mx_forceinline uint64_t mxa_load64(const uint64_t *src)
{
	// TODO: Try failing compare-exchange with result?
	uint64_t val;
	do {
		val = *src;
	} while ((uint64_t)_InterlockedCompareExchange64((volatile long long*)src, (long long)val, (long long)val) != val);
	return val;
}

mx_forceinline int mxa_cas64(uint64_t *dst, uint64_t cmp, uint64_t val)
{
	return (uint64_t)_InterlockedCompareExchange64((volatile long long*)dst, (long long)val, (long long)cmp) == cmp;
}

#define mxa_load_uptr(src) (*(const volatile uintptr_t*)(src))

mx_forceinline int mxa_cas_uptr(uintptr_t *dst, uintptr_t cmp, uintptr_t val)
{
	return (uintptr_t)_InterlockedCompareExchangePointer((void*volatile*)dst, (void*)val, (void*)cmp) == cmp;
}

#define mxa_load_ptr(src) (*(void*const volatile*)(src))

mx_forceinline int mxa_cas_ptr(void *dst, void *cmp, void *val)
{
	return _InterlockedCompareExchangePointer((void*volatile*)dst, val, cmp) == cmp;
}

mx_forceinline int mxa_double_cas(void *dst, const void *cmp, const void *val)
{
	long long lval = *(const long long*)val;
	long long lcmp = *(const long long*)cmp;
	return _InterlockedCompareExchange64((volatile long long*)dst, lval, lcmp) == lcmp;
}


#else
	#error "Unsupported architecture"
#endif
