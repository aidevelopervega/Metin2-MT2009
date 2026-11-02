#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64))
#include <intrin.h>
#include <immintrin.h>

extern "C" unsigned long long __fastcall XGETBV64(unsigned int num)
{
    return _xgetbv(num);
}

extern "C" unsigned long long __fastcall CPUID64(unsigned int func, unsigned int subfunc, unsigned int* output)
{
    int regs[4] = { 0, 0, 0, 0 };
    __cpuidex(regs, static_cast<int>(func), static_cast<int>(subfunc));
    output[0] = static_cast<unsigned int>(regs[0]);
    output[1] = static_cast<unsigned int>(regs[1]);
    output[2] = static_cast<unsigned int>(regs[2]);
    output[3] = static_cast<unsigned int>(regs[3]);
    return 1ULL;
}
#endif

