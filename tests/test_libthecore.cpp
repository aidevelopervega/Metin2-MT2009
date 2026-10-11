#include "stdafx.h"
#include <iostream>
#include <cstring>
#include <vector>

static int g_passCount = 0;
static int g_testCount = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        g_testCount++; \
        if (!(cond)) { \
            std::cerr << "[FAIL] Line " << __LINE__ << ": " << msg << std::endl; \
            return 1; \
        } else { \
            g_passCount++; \
        } \
    } while (0)

static void dummy_heartbeat(LPHEART ht, int pulse)
{
    (void)ht;
    (void)pulse;
}

int main()
{
#ifdef __WIN32__
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    std::cout << "=== LIBTHECORE AUTOMATED TEST SUITE ===" << std::endl;

    // ==========================================
    // 1. thecore_memcpy Tests
    // ==========================================
    {
        std::cout << "[RUN] Testing thecore_memcpy..." << std::endl;

        TEST_ASSERT(thecore_memcpy != NULL, "thecore_memcpy function pointer must not be null");

        char src[64];
        char dst[64];
        for (int i = 0; i < 64; ++i)
        {
            src[i] = (char)(i ^ 0x5A);
            dst[i] = 0;
        }

        void* ret = thecore_memcpy(dst, src, 64);
        TEST_ASSERT(ret == dst, "thecore_memcpy must return destination pointer");
        TEST_ASSERT(memcmp(dst, src, 64) == 0, "thecore_memcpy 64-byte payload must match exactly");

        // Boundary: 0 bytes copy
        char dummyDst = 'A';
        char dummySrc = 'B';
        ret = thecore_memcpy(&dummyDst, &dummySrc, 0);
        TEST_ASSERT(ret == &dummyDst, "thecore_memcpy 0 bytes must return dest pointer");
        TEST_ASSERT(dummyDst == 'A', "thecore_memcpy 0 bytes must not modify destination byte");

        // Boundary: 1 byte copy
        thecore_memcpy(&dummyDst, &dummySrc, 1);
        TEST_ASSERT(dummyDst == 'B', "thecore_memcpy 1 byte must copy correctly");
    }

    // ==========================================
    // 2. Buffer System Tests
    // ==========================================
    {
        std::cout << "[RUN] Testing buffer management..." << std::endl;

        LPBUFFER buf = buffer_new(64);
        TEST_ASSERT(buf != NULL, "buffer_new(64) must return non-null");
        TEST_ASSERT(buffer_size(buf) == 0, "Initial buffer size must be 0");
        TEST_ASSERT(buffer_has_space(buf) >= 64, "Initial buffer space must be >= 64");

        const char msg[] = "METIN2_MT2009_CORE_BUFFER";
        int msgLen = (int)strlen(msg);
        buffer_write(buf, msg, msgLen);
        TEST_ASSERT((int)buffer_size(buf) == msgLen, "Buffer size must match written length");

        const char* peekData = (const char*)buffer_read_peek(buf);
        TEST_ASSERT(peekData != NULL, "buffer_read_peek must return non-null");
        TEST_ASSERT(memcmp(peekData, msg, msgLen) == 0, "Peek data must match written data");
        TEST_ASSERT((int)buffer_size(buf) == msgLen, "Peek must not consume buffer content");

        char readBuf[64] = { 0 };
        buffer_read(buf, readBuf, msgLen);
        TEST_ASSERT(memcmp(readBuf, msg, msgLen) == 0, "buffer_read data must match original");
        TEST_ASSERT(buffer_size(buf) == 0, "Buffer size must be 0 after reading all data");

        // Auto expansion test
        std::vector<char> largeData(256, 'X');
        buffer_write(buf, largeData.data(), (int)largeData.size());
        TEST_ASSERT((int)buffer_size(buf) == 256, "Buffer must expand and hold 256 bytes");

        buffer_reset(buf);
        TEST_ASSERT(buffer_size(buf) == 0, "buffer_reset must reset data size to 0");

        buffer_delete(buf);
    }

    // ==========================================
    // 3. Cryptography Tests (TEA, DES, GOST)
    // ==========================================
    {
        std::cout << "[RUN] Testing ciphers (TEA, DES, GOST)..." << std::endl;

        // 3.1 TEA
        DWORD teaKey[4] = { 0x12345678, 0x9ABCDEF0, 0x0FEDCBA9, 0x87654321 };
        DWORD teaSrc[4] = { 0xDEADBEEF, 0xCAFEBABE, 0x01234567, 0x89ABCDEF };
        DWORD teaEnc[4] = { 0 };
        DWORD teaDec[4] = { 0 };

        TEA_Encrypt(teaEnc, teaSrc, teaKey, sizeof(teaSrc));
        TEST_ASSERT(memcmp(teaEnc, teaSrc, sizeof(teaSrc)) != 0, "TEA_Encrypt must alter plaintext");
        TEA_Decrypt(teaDec, teaEnc, teaKey, sizeof(teaEnc));
        TEST_ASSERT(memcmp(teaDec, teaSrc, sizeof(teaSrc)) == 0, "TEA_Decrypt must restore plaintext exactly");

        // 3.2 DES
        DWORD desKey[32];
        for (int k = 0; k < 32; ++k)
            desKey[k] = 0x01234567 ^ ((DWORD)k * 0x04030201);
        DWORD desIV1[2] = { 0x11223344, 0x55667788 };
        DWORD desIV2[2] = { 0x11223344, 0x55667788 };
        DWORD desSrc[2] = { 0xAABBCCDD, 0x11223344 };
        DWORD desEnc[2] = { 0 };
        DWORD desDec[2] = { 0 };

        DES_Encrypt(desEnc, desSrc, desKey, 8, desIV1);
        TEST_ASSERT(memcmp(desEnc, desSrc, 8) != 0, "DES_Encrypt must alter plaintext");
        DES_Decrypt(desDec, desEnc, desKey, 8, desIV2);
        TEST_ASSERT(memcmp(desDec, desSrc, 8) == 0, "DES_Decrypt must restore plaintext exactly");

        // 3.3 GOST
        DWORD gostKey[8] = { 10, 20, 30, 40, 50, 60, 70, 80 };
        DWORD gostIV1[2] = { 0x12345678, 0x90ABCDEF };
        DWORD gostIV2[2] = { 0x12345678, 0x90ABCDEF };
        DWORD gostSrc[2] = { 0xFEEDFACE, 0xBAADF00D };
        DWORD gostEnc[2] = { 0 };
        DWORD gostDec[2] = { 0 };

        GOST_Encrypt(gostEnc, gostSrc, gostKey, 8, gostIV1);
        TEST_ASSERT(memcmp(gostEnc, gostSrc, 8) != 0, "GOST_Encrypt must alter plaintext");
        GOST_Decrypt(gostDec, gostEnc, gostKey, 8, gostIV2);
        TEST_ASSERT(memcmp(gostDec, gostSrc, 8) == 0, "GOST_Decrypt must restore plaintext exactly");
    }

    // ==========================================
    // 4. Math & Number Utilities
    // ==========================================
    {
        std::cout << "[RUN] Testing math and number utilities..." << std::endl;

        TEST_ASSERT(MAX(15, 42) == 42, "MAX(15, 42) must be 42");
        TEST_ASSERT(MIN(15, 42) == 15, "MIN(15, 42) must be 15");
        TEST_ASSERT(MINMAX(10, 5, 50) == 10, "MINMAX(10, 5, 50) clamp to min must be 10");
        TEST_ASSERT(MINMAX(10, 25, 50) == 25, "MINMAX(10, 25, 50) inside range must be 25");
        TEST_ASSERT(MINMAX(10, 80, 50) == 50, "MINMAX(10, 80, 50) clamp to max must be 50");

        // number() random range
        for (int i = 0; i < 50; ++i)
        {
            int val = number(10, 20);
            TEST_ASSERT(val >= 10 && val <= 20, "number(10, 20) must always be in [10, 20]");
        }

        // Boundary: identical min and max
        TEST_ASSERT(number(7, 7) == 7, "number(7, 7) must equal 7");

        // Boundary: inverted range
        int inverted = number(20, 10);
        TEST_ASSERT(inverted >= 10 && inverted <= 20, "number(20, 10) must handle inverted range");

        // fnumber()
        float fval = fnumber(1.5f, 3.5f);
        TEST_ASSERT(fval >= 1.5f && fval <= 3.5f, "fnumber(1.5, 3.5) must be in [1.5, 3.5]");
    }

    // ==========================================
    // 5. Time & Timer Utilities
    // ==========================================
    {
        std::cout << "[RUN] Testing time utilities..." << std::endl;

        DWORD tStart = get_dword_time();
        thecore_msleep(15);
        DWORD tEnd = get_dword_time();
        TEST_ASSERT(tEnd >= tStart, "get_dword_time must be monotonic non-decreasing");

        float fTime1 = get_float_time();
        thecore_msleep(10);
        float fTime2 = get_float_time();
        TEST_ASSERT(fTime2 >= fTime1, "get_float_time must be monotonic non-decreasing");

        struct timeval tvA = { 10, 500000 };
        struct timeval tvB = { 4, 200000 };
        struct timeval* diff = timediff(&tvA, &tvB);
        TEST_ASSERT(diff->tv_sec == 6 && diff->tv_usec == 300000, "timediff must compute 6s 300000us");

        struct timeval* sum = timeadd(&tvA, &tvB);
        TEST_ASSERT(sum->tv_sec == 14 && sum->tv_usec == 700000, "timeadd must compute 14s 700000us");
    }

    // ==========================================
    // 6. String & Text Processing
    // ==========================================
    {
        std::cout << "[RUN] Testing string utilities..." << std::endl;

        char buf[16];
        strlcpy(buf, "HelloWorld123456789", sizeof(buf));
        TEST_ASSERT(strlen(buf) == 15, "strlcpy must truncate to size-1");
        TEST_ASSERT(buf[15] == '\0', "strlcpy must ensure null termination");

        char catBuf[16];
        strlcpy(catBuf, "foo", sizeof(catBuf));
        strlcat(catBuf, "bar", sizeof(catBuf));
        TEST_ASSERT(strcmp(catBuf, "foobar") == 0, "strlcat must concatenate foo + bar");

        char lowerBuf[32];
        lower_string("Metin2 SERVER", lowerBuf, sizeof(lowerBuf));
        TEST_ASSERT(strcmp(lowerBuf, "metin2 server") == 0, "lower_string must lowercase ASCII");

        char trimBuf[32];
        trim_and_lower("   WARRIOR_NINJA   ", trimBuf, sizeof(trimBuf));
        TEST_ASSERT(strcmp(trimBuf, "warrior_ninja") == 0, "trim_and_lower must trim and lowercase");

        TEST_ASSERT(is_abbrev((char*)"att", (char*)"attack") == 1, "att is abbrev of attack");
        TEST_ASSERT(is_abbrev((char*)"def", (char*)"attack") == 0, "def is not abbrev of attack");
    }

    // ==========================================
    // 7. Fdwatch Infrastructure
    // ==========================================
    {
        std::cout << "[RUN] Testing fdwatch..." << std::endl;

        LPFDWATCH fdw = fdwatch_new(16);
        TEST_ASSERT(fdw != NULL, "fdwatch_new(16) must succeed");

        socket_t s = socket(AF_INET, SOCK_STREAM, 0);
        if (s != INVALID_SOCKET)
        {
            fdwatch_add_fd(fdw, s, NULL, FDW_READ, 0);
            struct timeval tv = { 0, 0 };
            int r = fdwatch(fdw, &tv);
            TEST_ASSERT(r >= 0, "fdwatch select must return valid event count >= 0");

            fdwatch_del_fd(fdw, s);
            socket_close(s);
        }

        fdwatch_delete(fdw);
    }

    // ==========================================
    // 8. Sockets & Binding
    // ==========================================
    {
        std::cout << "[RUN] Testing sockets..." << std::endl;

        socket_t s = socket_tcp_bind("127.0.0.1", 0);
        TEST_ASSERT(s != INVALID_SOCKET, "socket_tcp_bind on port 0 must succeed");
        socket_close(s);
    }

    // ==========================================
    // 9. Heart Subsystem
    // ==========================================
    {
        std::cout << "[RUN] Testing heart subsystem..." << std::endl;

        LPHEART ht = heart_new(100000, dummy_heartbeat);
        TEST_ASSERT(ht != NULL, "heart_new(100000) must succeed");
        TEST_ASSERT(ht->passes_per_sec == 10, "heart passes_per_sec for 100000us must be 10");
        heart_delete(ht);
    }

    std::cout << "\n[SUCCESS] libthecore: " << g_passCount << "/" << g_testCount
              << " tests passed cleanly!" << std::endl;

#ifdef __WIN32__
    WSACleanup();
#endif
    return 0;
}
