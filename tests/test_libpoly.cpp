#include <iostream>
#include <cassert>
#include <cmath>
#include "Poly.h"

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

int main()
{
    std::cout << "=== LIBPOLY AUTOMATED TEST SUITE ===" << std::endl;

    CPoly poly;

    // 1. Normal Formula Evaluations
    {
        TEST_ASSERT(poly.Analyze("10 + 20") != 0, "Analyze simple addition");
        TEST_ASSERT(poly.Eval() == 30.0, "Evaluate 10 + 20");

        TEST_ASSERT(poly.Analyze("b * 2 + k") != 0, "Analyze formula with variables");
        poly.SetVar("b", 15);
        poly.SetVar("k", 20);
        TEST_ASSERT(poly.Eval() == 50.0, "Evaluate b*2+k with b=15, k=20");

        TEST_ASSERT(poly.Analyze("min(10, 20) + max(5, 1)") != 0, "Analyze min/max functions");
        TEST_ASSERT(poly.Eval() == 15.0, "Evaluate min(10, 20) + max(5, 1)");

        TEST_ASSERT(poly.Analyze("(10 + 20) * 3 - 30 / 2") != 0, "Analyze precedence and parentheses");
        TEST_ASSERT(poly.Eval() == 75.0, "Evaluate (10 + 20) * 3 - 30 / 2");

        TEST_ASSERT(poly.Analyze("sqrt(16) + 6") != 0, "Analyze sqrt function");
        TEST_ASSERT(poly.Eval() == 10.0, "Evaluate sqrt(16) + 6");
    }

    // 2. Boundary Cases
    {
        // Multiply by zero
        TEST_ASSERT(poly.Analyze("b * 0 + 0") != 0, "Analyze multiply by zero");
        poly.SetVar("b", 999);
        TEST_ASSERT(poly.Eval() == 0.0, "Evaluate b * 0 + 0");

        // Single integer literal
        TEST_ASSERT(poly.Analyze("42") != 0, "Analyze single constant");
        TEST_ASSERT(poly.Eval() == 42.0, "Evaluate single constant");

        // Negative numbers and subtraction
        TEST_ASSERT(poly.Analyze("-10 + 5") != 0, "Analyze leading negative number");
        TEST_ASSERT(poly.Eval() == -5.0, "Evaluate -10 + 5");

        // Clear and reuse
        poly.Clear();
        TEST_ASSERT(poly.Analyze("100 / 4") != 0, "Analyze after Clear");
        TEST_ASSERT(poly.Eval() == 25.0, "Evaluate after Clear");

        // GetVar check
        poly.Clear();
        poly.Analyze("x + y");
        poly.SetVar("x", 12.5);
        TEST_ASSERT(poly.GetVar("x") == 12.5, "GetVar check");
    }

    // 3. Error and Invalid Syntax Cases
    {
        // Unmatched parenthesis
        TEST_ASSERT(poly.Analyze("b * (2 + k") == 0, "Reject unmatched parenthesis");

        // Invalid operator juxtaposition
        TEST_ASSERT(poly.Analyze("b + * 2") == 0, "Reject invalid operators");

        // Empty string is accepted as a valid empty expression in Metin2, evaluates to 0
        TEST_ASSERT(poly.Analyze("") != 0, "Analyze empty string (no-op)");
        TEST_ASSERT(poly.Eval() == 0.0, "Evaluate empty string returns 0");

        // Only operators
        TEST_ASSERT(poly.Analyze("+++") == 0, "Reject only operators");
    }

    std::cout << "[SUCCESS] libpoly: " << g_passCount << "/" << g_testCount << " tests passed cleanly!" << std::endl;
    return 0;
}
