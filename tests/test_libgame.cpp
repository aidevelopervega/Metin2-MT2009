#include "stdafx.h"
#include <iostream>
#include <vector>
#include "grid.h"
#include "attribute.h"
#include "targa.h"

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
    std::cout << "=== LIBGAME AUTOMATED TEST SUITE ===" << std::endl;

    // ==========================================
    // 1. CGrid Tests
    // ==========================================
    {
        std::cout << "[RUN] Testing CGrid..." << std::endl;

        // Normal: 5x5 grid
        CGrid grid(5, 5);
        TEST_ASSERT(grid.GetSize() == 25, "CGrid 5x5 size must be 25");
        TEST_ASSERT(grid.IsEmpty(0, 2, 2) == true, "Position 0 must be empty initially");
        TEST_ASSERT(grid.Put(0, 2, 2) == true, "Put 2x2 item at position 0 must succeed");
        TEST_ASSERT(grid.IsEmpty(0, 2, 2) == false, "Position 0 must NOT be empty after Put");
        TEST_ASSERT(grid.IsEmpty(1, 1, 1) == false, "Occupied cell (1,0) must not be empty");

        // Find next blank
        int blank = grid.FindBlank(2, 2);
        TEST_ASSERT(blank >= 2, "Next blank space for 2x2 must be at index >= 2");

        // Boundary: Cannot put overlapping or out-of-grid item
        TEST_ASSERT(grid.Put(0, 1, 1) == false, "Cannot put on already occupied position");
        TEST_ASSERT(grid.Put(24, 2, 2) == false, "Cannot put 2x2 at position 24 in 5x5 grid");
        TEST_ASSERT(grid.Put(4, 2, 1) == false, "Cannot put item that wraps around row edge");

        // Boundary: Invalid / Negative positions
        TEST_ASSERT(grid.IsEmpty(-1, 1, 1) == false, "Negative pos must be reported not empty / invalid");
        TEST_ASSERT(grid.IsEmpty(100, 1, 1) == false, "Out-of-bounds pos must be reported not empty");

        // Get (remove) and Clear
        grid.Get(0, 2, 2);
        TEST_ASSERT(grid.IsEmpty(0, 2, 2) == true, "Position 0 must be empty after Get");
        grid.Put(5, 1, 1);
        grid.Clear();
        TEST_ASSERT(grid.IsEmpty(5, 1, 1) == true, "Grid must be completely clear after Clear()");

        // 1x1 Boundary Grid
        CGrid tinyGrid(1, 1);
        TEST_ASSERT(tinyGrid.GetSize() == 1, "1x1 grid size is 1");
        TEST_ASSERT(tinyGrid.Put(0, 1, 1) == true, "Put 1x1 on 1x1 grid");
        TEST_ASSERT(tinyGrid.Put(0, 1, 1) == false, "Cannot put 1x1 again");
    }

    // ==========================================
    // 2. CAttribute Tests
    // ==========================================
    {
        std::cout << "[RUN] Testing CAttribute..." << std::endl;

        // Normal: 10x10 attribute map
        CAttribute attr(10, 10);
        TEST_ASSERT(attr.Get(3, 4) == 0, "Initial attribute at (3,4) must be 0");
        attr.Set(3, 4, 0x1234);
        TEST_ASSERT(attr.Get(3, 4) == 0x1234, "Attribute at (3,4) must match set value");
        attr.Remove(3, 4, 0x1000);
        TEST_ASSERT(attr.Get(3, 4) == 0x0234, "Attribute at (3,4) after Remove bitmask");
        attr.Remove(3, 4, 0x0234);
        TEST_ASSERT(attr.Get(3, 4) == 0, "Attribute at (3,4) must be 0 after full Remove");

        // Boundary: Coordinates at exact edges (0, 0) and (9, 9)
        attr.Set(0, 0, 0xFF);
        TEST_ASSERT(attr.Get(0, 0) == 0xFF, "Attribute at (0,0) edge");
        attr.Set(9, 9, 0xAA);
        TEST_ASSERT(attr.Get(9, 9) == 0xAA, "Attribute at (9,9) edge");

        // Boundary: Out-of-bounds coordinates (should not crash, Get returns 0)
        attr.Set(100, 100, 0x55);
        TEST_ASSERT(attr.Get(100, 100) == 0, "Out-of-bounds Get must return default 0");

        // CopyRow test
        DWORD rowBuffer[10] = {0};
        attr.CopyRow(0, rowBuffer);
        TEST_ASSERT(rowBuffer[0] == 0xFF, "CopyRow must copy row elements accurately");

        // Array constructor
        std::vector<DWORD> rawData(16, 0x01);
        CAttribute attrFromArray(rawData.data(), 4, 4);
        TEST_ASSERT(attrFromArray.Get(2, 2) == 0x01, "Array constructor attribute check");
    }

    // ==========================================
    // 3. CTargaImage Tests
    // ==========================================
    {
        std::cout << "[RUN] Testing CTargaImage..." << std::endl;

        CTargaImage tga;
        tga.Create(32, 32);

        char* pBase = tga.GetBasePointer(0);
        TEST_ASSERT(pBase != nullptr, "Base pointer line 0 must not be null");

        char* pLastLine = tga.GetBasePointer(31);
        TEST_ASSERT(pLastLine != nullptr, "Base pointer line 31 must not be null");
        TEST_ASSERT(pLastLine > pBase, "Last line pointer must be greater than base pointer");
    }

    std::cout << "[SUCCESS] libgame: " << g_passCount << "/" << g_testCount << " tests passed cleanly!" << std::endl;
    return 0;
}
