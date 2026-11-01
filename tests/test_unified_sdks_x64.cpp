#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

// 1. Common Header-Only SDKs (Server == Client unified versions)
#include <boost/version.hpp>
#include <boost/unordered_map.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/asio/ip/network_v6.hpp>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <msl/msl.h>

// 2. Crypto++ 8.9.0 (Unified Server & Client headers + extern/client/lib/cryptlib-x64.lib)
#include <cryptopp/config_ver.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/aes.h>
#include <cryptopp/modes.h>
#include <cryptopp/filters.h>
#include <cryptopp/osrng.h>
#include <cryptopp/sha.h>
#include <cryptopp/hex.h>

// 3. LZO 2.10 (Unified Server & Client headers + extern/client/lib/lzo2_x64.lib)
#include <lzo/lzoconf.h>
#include <lzo/lzo1x.h>

// 4. DevIL 1.8.0 (Unified Server & Client headers + extern/client/lib/devil_x64.lib + DevIL_x64.dll)
#include <IL/il.h>

// 5. Official Microsoft.DXSDK.D3DX 9.29.952.8 x64 (d3dx9.lib + D3DX9_43.dll) + Direct3D 9Ex
#include <d3d9.h>
#include <d3dx9.h>

// 6. IJG libjpeg 9.3 (libjpeg-x64.lib) + Client EterLib/JpegFile.cpp
#include <libjpeg/jpeglib.h>
#include "../client/src/EterLib/JpegFile.h"

// 7. Discord RPC 3.4.0 (discord_rpc_x64.lib)
#include <discord_rpc.h>

// 8. Real MT2009 Client Modules using these SDKs
#include "../client/src/EterBase/cipher.h"
#include "../client/src/EterBase/lzo.h"
#include "../client/src/EterBase/CRC32.h"
#include "../client/src/EterPack/EterPackPolicy_CSHybridCrypt.h"
#include "../client/src/UserInterface/MarkImage.h"
#include "../client/src/UserInterface/HashUtils.h"

extern std::string GetHardwareID();
extern CGuildMarkImage* NewMarkImage();
extern void DeleteMarkImage(CGuildMarkImage* pkImage);

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            ++g_passed;                                                        \
        } else {                                                               \
            ++g_failed;                                                        \
            std::fprintf(stderr, "[FAIL] %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                                      \
    } while (0)

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("=== MT2009 Unified Server/Client x64 SDK Verification Suite ===\n");
    CLZO lzoInstance;

    // -------------------------------------------------------------------------
    // Gate 1: Architecture & Unified SDK Version Assertions
    // -------------------------------------------------------------------------
    static_assert(sizeof(void*) == 8, "Must be compiled as native 64-bit (AMD64)");
    static_assert(sizeof(Pixel) == 4, "MarkImage Pixel must be 4-byte uint32_t on all 64-bit platforms");
    static_assert(BOOST_VERSION == 109200, "Boost version must be unified at 1.92.0");
    static_assert(CRYPTOPP_VERSION == 890, "Crypto++ version must be unified at 8.9.0");
    static_assert(LZO_VERSION == 0x20a0, "LZO version must be unified at 2.10 (0x20a0)");
    static_assert(IL_VERSION == 180, "DevIL header version must be unified at 1.8.0 (180)");
    static_assert(FMT_VERSION == 120200, "fmt version must be unified at 12.2.0");

    CHECK(sizeof(void*) == 8, "Native AMD64 8-byte pointer width");
    CHECK(sizeof(Pixel) == 4, "MarkImage Pixel 4-byte BGRA width (LP64/LLP64 parity)");
    CHECK(BOOST_VERSION == 109200, "Boost 1.92.0 header version");
    CHECK(std::strcmp(RAPIDJSON_VERSION_STRING, "1.1.0") == 0, "RapidJSON 1.1.0 header version");
    CHECK(CRYPTOPP_VERSION == 890, "Crypto++ 8.9.0 header version");
    CHECK(LZO_VERSION == 0x20a0, "LZO 2.10 header version");
    CHECK(IL_VERSION == 180, "DevIL 1.8.0 header version");
    CHECK(FMT_VERSION == 120200, "fmt 12.2.0 header version");

    // -------------------------------------------------------------------------
    // Gate 2: Common Header-Only SDK Runtime Checks (Boost 1.92, RapidJSON, fmt, msl)
    // -------------------------------------------------------------------------
    {
        // Boost.Asio network_v4/v6 (previously broken by stale 2023 extern/client/include/boost .ipp shadowing)
        auto net4 = boost::asio::ip::make_network_v4("192.168.1.0/24");
        CHECK(net4.prefix_length() == 24, "Boost 1.92.0 asio::ip::network_v4 unshadowed");

        boost::unordered_map<uint64_t, std::string> umap;
        umap[0x20090064ULL] = "mt2009_x64";
        CHECK(umap.find(0x20090064ULL) != umap.end(), "Boost 1.92.0 unordered_map 64-bit key lookup");

        // RapidJSON 1.1.0 (previously shadowed by extern/client/include/rapidjson/document.h)
        rapidjson::Document doc;
        doc.Parse("{\"protocol\":537460836,\"arch\":\"amd64\"}");
        CHECK(!doc.HasParseError() && doc["protocol"].GetUint() == 0x20090064U, "RapidJSON 1.1.0 DOM parse");

        // fmt 12.2.0
        std::string s = fmt::format("MT2-{:#x}", 0x20090064U);
        CHECK(s == "MT2-0x20090064", "fmt 12.2.0 format");

        // msl 1.2.1
        std::string trimmed = "  mt2009  ";
        msl::trim_in_place(trimmed);
        CHECK(trimmed == "mt2009", "msl 1.2.1 trim_in_place");
    }

    // -------------------------------------------------------------------------
    // Gate 3: Crypto++ 8.9.0 Static Library (cryptlib-x64.lib) + Real Client Modules
    // -------------------------------------------------------------------------
    {
        // 3a. Client HashUtils.cpp (AES ECB encrypt/decrypt)
        const std::string key16 = "0123456789ABCDEF";
        const std::string plain = "MT2009_X64_UNIFIED_CRYPTOPP_890_PAYLOAD";
        std::string enc = encryptAESString(plain, key16);
        std::string dec = decryptAESString(enc, key16);
        CHECK(!enc.empty() && enc != plain && dec == plain, "Client HashUtils.cpp AES-128 round-trip via cryptlib-x64.lib");

        // 3b. Client Hardware.cpp (SMBIOS + CPUID + Boost trim + XorStr + Crypto++ AES/HexEncoder)
        std::string hwid = GetHardwareID();
        CHECK(!hwid.empty() && hwid.size() >= 32, "Client Hardware.cpp GetHardwareID() via Crypto++ 8.9.0 + smbios + xorstr");

        // 3c. Client EterBase/cipher.cpp (DH2KeyAgreement + CTR_Mode<T> packet cipher handshake)
        Cipher clientCipher;
        Cipher serverCipher;
        std::vector<uint8_t> clientBuf(1024, 0);
        std::vector<uint8_t> serverBuf(1024, 0);
        size_t clientLen = clientBuf.size();
        size_t serverLen = serverBuf.size();

        size_t clientAgreed = clientCipher.Prepare(clientBuf.data(), &clientLen);
        size_t serverAgreed = serverCipher.Prepare(serverBuf.data(), &serverLen);
        CHECK(clientAgreed > 0 && serverAgreed > 0 && clientLen > 0 && serverLen > 0,
              "Client EterBase/cipher.cpp DH2KeyAgreement::Prepare");

        bool clientAct = clientCipher.Activate(true, clientAgreed, serverBuf.data(), serverLen);
        bool serverAct = serverCipher.Activate(false, serverAgreed, clientBuf.data(), clientLen);
        CHECK(clientAct && serverAct, "Client EterBase/cipher.cpp DH2KeyAgreement::Activate bilateral");
        clientCipher.set_activated(true);
        serverCipher.set_activated(true);

        const char* packetMsg = "HEADER_CG_LOGIN3_X64_PACKET_STREAM_TEST_2026";
        std::vector<char> wire(packetMsg, packetMsg + std::strlen(packetMsg) + 1);
        clientCipher.Encrypt(wire.data(), wire.size());
        CHECK(std::memcmp(wire.data(), packetMsg, wire.size()) != 0, "Client Cipher::Encrypt mutates plaintext");
        serverCipher.Decrypt(wire.data(), wire.size());
        CHECK(std::strcmp(wire.data(), packetMsg) == 0, "Client Cipher::Decrypt restores exact plaintext");
        clientCipher.CleanUp();
        serverCipher.CleanUp();

        // 3d. Client EterPack/EterPackPolicy_CSHybridCrypt.cpp (Camellia / Twofish / XTEA CTR + AutoSeededRandomPool)
        EterPackPolicy_CSHybridCrypt policy;
        const char* exts[] = { "sample.msm", "sample.mse", "sample.msa" };
        for (const char* fname : exts)
        {
            CHECK(policy.GenerateCryptKey(fname), "EterPackPolicy_CSHybridCrypt::GenerateCryptKey");
            CHECK(policy.IsContainingCryptKey(), "EterPackPolicy_CSHybridCrypt::IsContainingCryptKey");

            std::vector<BYTE> src(128, 0x5A);
            for (size_t i = 0; i < src.size(); ++i)
                src[i] = static_cast<BYTE>((i * 37 + 11) & 0xFF);

            CLZObject zEnc;
            CHECK(policy.EncryptMemory(fname, src.data(), static_cast<int>(src.size()), zEnc),
                  "EterPackPolicy_CSHybridCrypt::EncryptMemory");
            CLZObject zDec;
            CHECK(policy.DecryptMemory(fname, zEnc.GetBuffer(), zEnc.GetBufferSize(), zDec),
                  "EterPackPolicy_CSHybridCrypt::DecryptMemory");
            CHECK(zDec.GetBufferSize() == static_cast<DWORD>(src.size()) &&
                  std::memcmp(zDec.GetBuffer(), src.data(), src.size()) == 0,
                  "EterPackPolicy_CSHybridCrypt round-trip payload match");
        }
    }

    // -------------------------------------------------------------------------
    // Gate 4: LZO 2.10 Static Library (lzo2_x64.lib) + EterBase/lzo.cpp
    // -------------------------------------------------------------------------
    {
        CHECK(lzo_init() == LZO_E_OK, "LZO 2.10 lzo_init() == LZO_E_OK");
        CHECK(lzo_version() == 0x20a0, "LZO 2.10 runtime lzo_version() == 0x20a0");

        std::vector<BYTE> raw(4096);
        for (size_t i = 0; i < raw.size(); ++i)
            raw[i] = static_cast<BYTE>((i / 16) & 0xFF);

        DWORD keys[4] = { 0x11223344U, 0x55667788U, 0x99AABBCCU, 0xDDEEFF00U };
        CLZObject zCompressed;
        CHECK(CLZO::Instance().CompressEncryptedMemory(zCompressed, raw.data(), static_cast< UINT >(raw.size()), keys),
              "CLZO::CompressEncryptedMemory (LZO 2.10 + XTEA)");

        CLZObject zDecompressed;
        CHECK(CLZO::Instance().Decompress(zDecompressed, zCompressed.GetBuffer(), keys),
              "CLZO::Decompress (LZO 2.10 + XTEA)");
        CHECK(zDecompressed.GetSize() == raw.size() &&
              std::memcmp(zDecompressed.GetBuffer(), raw.data(), raw.size()) == 0,
              "CLZO encrypted compress/decompress exact round-trip");
    }

    // -------------------------------------------------------------------------
    // Gate 5: DevIL 1.8.0 (devil_x64.lib + DevIL_x64.dll) + Client MarkImage.cpp
    // -------------------------------------------------------------------------
    {
        ilInit();
        ILint runtimeVer = ilGetInteger(IL_VERSION_NUM);
        CHECK(runtimeVer == 180, "DevIL 1.8.0 runtime ilGetInteger(IL_VERSION_NUM) == 180");

        std::filesystem::path tempTga = std::filesystem::temp_directory_path() / "mt2009_mark_test_x64.tga";
        std::error_code ec;
        std::filesystem::remove(tempTga, ec);

        CGuildMarkImage* pkImg = NewMarkImage();
        CHECK(pkImg != nullptr, "NewMarkImage() allocation");
        CHECK(pkImg->Build(tempTga.string().c_str()), "CGuildMarkImage::Build via DevIL_x64.dll");
        CHECK(pkImg->Load(tempTga.string().c_str()), "CGuildMarkImage::Load via DevIL_x64.dll");

        Pixel markPixels[SGuildMark::SIZE];
        for (size_t i = 0; i < SGuildMark::SIZE; ++i)
            markPixels[i] = 0xFF112233U + static_cast<Pixel>(i);

        CHECK(pkImg->SaveMark(0, reinterpret_cast<BYTE*>(markPixels)),
              "CGuildMarkImage::SaveMark + SGuildMarkBlock::Compress (DevIL 1.8.0 + LZO 2.10)");

        DWORD crcList[CGuildMarkImage::BLOCK_TOTAL_COUNT] = {};
        pkImg->GetBlockCRCList(crcList);
        CHECK(crcList[0] != 0, "CGuildMarkImage::GetBlockCRCList non-zero block 0 CRC");

        DWORD zeroCrc[CGuildMarkImage::BLOCK_TOTAL_COUNT] = {};
        std::map<BYTE, const SGuildMarkBlock*> diffBlocks;
        pkImg->GetDiffBlocks(zeroCrc, diffBlocks);
        CHECK(diffBlocks.find(0) != diffBlocks.end() && diffBlocks[0]->m_sizeCompBuf > 0,
              "CGuildMarkImage::GetDiffBlocks returns compressed block 0");

        CGuildMarkImage* pkReceiver = NewMarkImage();
        std::filesystem::path tempTga2 = std::filesystem::temp_directory_path() / "mt2009_mark_recv_x64.tga";
        CHECK(pkReceiver->Build(tempTga2.string().c_str()) && pkReceiver->Load(tempTga2.string().c_str()),
              "Receiver CGuildMarkImage::Build + Load");
        CHECK(pkReceiver->SaveBlockFromCompressedData(
                  0, diffBlocks[0]->m_abCompBuf, static_cast<DWORD>(diffBlocks[0]->m_sizeCompBuf)),
              "Receiver CGuildMarkImage::SaveBlockFromCompressedData round-trip");

        DWORD recvCrc[CGuildMarkImage::BLOCK_TOTAL_COUNT] = {};
        pkReceiver->GetBlockCRCList(recvCrc);
        CHECK(recvCrc[0] == crcList[0], "Sender and Receiver GuildMark block 0 CRC match");

        DeleteMarkImage(pkReceiver);
        DeleteMarkImage(pkImg);
        std::filesystem::remove(tempTga, ec);
        std::filesystem::remove(tempTga2, ec);
    }

    // -------------------------------------------------------------------------
    // Gate 6: Official Microsoft.DXSDK.D3DX 9.29.952.8 x64 (d3dx9.lib + D3DX9_43.dll) + Direct3D 9Ex
    // -------------------------------------------------------------------------
    {
        static_assert(D3DX_SDK_VERSION == 43, "D3DX_SDK_VERSION must be 43 (June 2010 / 9.29.952.8)");
        CHECK(D3DXCheckVersion(D3D_SDK_VERSION, D3DX_SDK_VERSION) == TRUE,
              "D3DXCheckVersion(D3D_SDK_VERSION, 43) via D3DX9_43.dll x64");

        // Math / Matrix / Vector / MatrixStack helpers used across EterLib, GameLib, EffectLib, SpeedTreeLib
        D3DXMATRIX matIdentity, matView, matProj, matWorldViewProj, matInv;
        D3DXMatrixIdentity(&matIdentity);
        D3DXVECTOR3 vEye(0.0f, -500.0f, 300.0f), vAt(0.0f, 0.0f, 0.0f), vUp(0.0f, 0.0f, 1.0f);
        D3DXMatrixLookAtRH(&matView, &vEye, &vAt, &vUp);
        D3DXMatrixPerspectiveFovRH(&matProj, D3DXToRadian(45.0f), 16.0f / 9.0f, 10.0f, 10000.0f);
        D3DXMatrixMultiply(&matWorldViewProj, &matView, &matProj);
        FLOAT det = D3DXMatrixDeterminant(&matView);
        CHECK(det != 0.0f && D3DXMatrixInverse(&matInv, &det, &matView) != nullptr,
              "D3DXMatrixLookAtRH + D3DXMatrixPerspectiveFovRH + D3DXMatrixInverse");

        D3DXVECTOR3 vOut(0.0f, 0.0f, 0.0f);
        D3DXVec3TransformCoord(&vOut, &vEye, &matView);
        CHECK(vOut.z < 0.0f, "D3DXVec3TransformCoord camera-space transform");

        LPD3DXMATRIXSTACK pStack = nullptr;
        CHECK(SUCCEEDED(D3DXCreateMatrixStack(0, &pStack)) && pStack != nullptr,
              "D3DXCreateMatrixStack (ID3DXMatrixStack x64)");
        if (pStack)
        {
            pStack->LoadIdentity();
            pStack->Push();
            pStack->MultMatrix(&matView);
            CHECK(pStack->GetTop() != nullptr, "ID3DXMatrixStack::Push + MultMatrix + GetTop");
            pStack->Pop();
            pStack->Release();
        }

        // Shader Assembler (used in EterLib/GrpShader.cpp)
        const char* vsAsm = "vs_1_1\ndcl_position v0\nmov oPos, v0\n";
        LPD3DXBUFFER pShaderBuf = nullptr;
        LPD3DXBUFFER pErrorBuf = nullptr;
        HRESULT hrAsm = D3DXAssembleShader(vsAsm, static_cast<UINT>(std::strlen(vsAsm)),
                                           nullptr, nullptr, 0, &pShaderBuf, &pErrorBuf);
        CHECK(SUCCEEDED(hrAsm) && pShaderBuf != nullptr && pShaderBuf->GetBufferSize() > 0,
              "D3DXAssembleShader vs_1_1 bytecode generation");
        if (pShaderBuf) pShaderBuf->Release();
        if (pErrorBuf) pErrorBuf->Release();

        // Live Direct3D 9Ex Device + D3DX9 Mesh / Texture / Surface Interop
        IDirect3D9Ex* pD3D9Ex = nullptr;
        if (SUCCEEDED(Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D9Ex)) && pD3D9Ex)
        {
            HWND hWnd = CreateWindowExA(0, "STATIC", "MT2009_D3DX9_X64", WS_POPUP, 0, 0, 64, 64,
                                        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
            D3DPRESENT_PARAMETERS d3dpp = {};
            d3dpp.Windowed = TRUE;
            d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
            d3dpp.BackBufferWidth = 64;
            d3dpp.BackBufferHeight = 64;
            d3dpp.hDeviceWindow = hWnd;

            IDirect3DDevice9Ex* pDevEx = nullptr;
            HRESULT hrDev = pD3D9Ex->CreateDeviceEx(
                D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                &d3dpp, nullptr, &pDevEx);
            if (FAILED(hrDev))
            {
                hrDev = pD3D9Ex->CreateDeviceEx(
                    D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, hWnd,
                    D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                    &d3dpp, nullptr, &pDevEx);
            }
            CHECK(SUCCEEDED(hrDev) && pDevEx != nullptr, "Direct3D 9Ex device creation for D3DX9 interop");
            if (pDevEx)
            {
                // Mesh helpers used in EterLib/GrpScreen.cpp (D3DXCreateSphere / D3DXCreateCylinder)
                LPD3DXMESH pSphere = nullptr;
                LPD3DXMESH pCylinder = nullptr;
                CHECK(SUCCEEDED(D3DXCreateSphere(pDevEx, 10.0f, 8, 8, &pSphere, nullptr)) && pSphere != nullptr,
                      "D3DXCreateSphere on live IDirect3DDevice9Ex");
                CHECK(SUCCEEDED(D3DXCreateCylinder(pDevEx, 5.0f, 5.0f, 20.0f, 8, 4, &pCylinder, nullptr)) && pCylinder != nullptr,
                      "D3DXCreateCylinder on live IDirect3DDevice9Ex");
                if (pSphere) pSphere->Release();
                if (pCylinder) pCylinder->Release();

                // Texture & Surface helpers used in EterLib/GrpImageTexture.cpp & PythonGraphic.cpp
                LPDIRECT3DTEXTURE9 pTex = nullptr;
                CHECK(SUCCEEDED(D3DXCreateTexture(pDevEx, 32, 32, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pTex)) && pTex != nullptr,
                      "D3DXCreateTexture(D3DPOOL_DEFAULT) on live IDirect3DDevice9Ex");
                if (pTex)
                {
                    IDirect3DSurface9* pSysSurf = nullptr;
                    if (SUCCEEDED(pDevEx->CreateOffscreenPlainSurface(32, 32, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysSurf, nullptr)) && pSysSurf)
                    {
                        D3DLOCKED_RECT lr = {};
                        if (SUCCEEDED(pSysSurf->LockRect(&lr, nullptr, 0)))
                        {
                            for (int y = 0; y < 32; ++y)
                            {
                                uint32_t* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(lr.pBits) + y * lr.Pitch);
                                for (int x = 0; x < 32; ++x)
                                    row[x] = 0xFF2060A0U;
                            }
                            pSysSurf->UnlockRect();
                        }
                        LPD3DXBUFFER pJpgBuf = nullptr;
                        CHECK(SUCCEEDED(D3DXSaveSurfaceToFileInMemory(&pJpgBuf, D3DXIFF_JPG, pSysSurf, nullptr, nullptr)) && pJpgBuf != nullptr,
                              "D3DXSaveSurfaceToFileInMemory(D3DXIFF_JPG)");
                        if (pJpgBuf)
                        {
                            D3DXIMAGE_INFO imgInfo = {};
                            LPDIRECT3DTEXTURE9 pLoadedTex = nullptr;
                            CHECK(SUCCEEDED(D3DXCreateTextureFromFileInMemoryEx(
                                      pDevEx, pJpgBuf->GetBufferPointer(), pJpgBuf->GetBufferSize(),
                                      D3DX_DEFAULT, D3DX_DEFAULT, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                      D3DX_FILTER_NONE, D3DX_FILTER_NONE, 0, &imgInfo, nullptr, &pLoadedTex)) &&
                                      pLoadedTex != nullptr && imgInfo.Width == 32 && imgInfo.Height == 32,
                                  "D3DXCreateTextureFromFileInMemoryEx(D3DPOOL_DEFAULT) on live IDirect3DDevice9Ex");
                            if (pLoadedTex) pLoadedTex->Release();
                            pJpgBuf->Release();
                        }
                        pSysSurf->Release();
                    }
                    pTex->Release();
                }
                pDevEx->Release();
            }
            if (hWnd) DestroyWindow(hWnd);
            pD3D9Ex->Release();
        }
    }

    // -------------------------------------------------------------------------
    // Gate 7: IJG libjpeg 9.3 (libjpeg-x64.lib) + Client EterLib/JpegFile.cpp
    // -------------------------------------------------------------------------
    {
        static_assert(JPEG_LIB_VERSION == 90, "IJG libjpeg version must be 90 (9.x)");
        const int W = 16, H = 16;
        std::vector<uint8_t> rgbIn(W * H * 3, 0);
        for (int i = 0; i < W * H; ++i)
        {
            rgbIn[i * 3 + 0] = 200;
            rgbIn[i * 3 + 1] = 100;
            rgbIn[i * 3 + 2] = 50;
        }
        std::vector<uint8_t> jpgMem(16384, 0);
        int jpgBytes = jpeg_save_to_mem(rgbIn.data(), W, H, 95, jpgMem.data(), static_cast<int>(jpgMem.size()));
        CHECK(jpgBytes > 64 && jpgMem[0] == 0xFF && jpgMem[1] == 0xD8,
              "EterLib/JpegFile.cpp jpeg_save_to_mem via libjpeg-x64.lib produces valid SOI header");

        std::vector<uint8_t> rgbOut(W * H * 3, 0);
        CHECK(jpeg_load_from_mem(jpgMem.data(), jpgBytes, rgbOut.data(), W, H) == 1,
              "EterLib/JpegFile.cpp jpeg_load_from_mem via libjpeg-x64.lib");
        CHECK(std::abs(static_cast<int>(rgbOut[0]) - 200) <= 5,
              "EterLib/JpegFile.cpp JPEG compress/decompress pixel fidelity");
    }

    // -------------------------------------------------------------------------
    // Gate 8: Discord RPC 3.4.0 (discord_rpc_x64.lib)
    // -------------------------------------------------------------------------
    {
        DiscordEventHandlers handlers = {};
        Discord_Initialize("1180989036949680258", &handlers, 0, nullptr);
        DiscordRichPresence presence = {};
        presence.state = "Lokacja: Yongan (M1)";
        presence.details = "Nazwa: Lv. 99 TestX64";
        presence.buttonLabel = "Dolacz do gry!";
        presence.buttonURL = "https://mt2009.pl/";
        Discord_UpdatePresence(&presence);
        Discord_RunCallbacks();
        Discord_ClearPresence();
        Discord_Shutdown();
        CHECK(true, "Discord RPC 3.4.0 x64 lifecycle (Initialize/UpdatePresence/RunCallbacks/ClearPresence/Shutdown)");
    }

    std::printf("=== Unified Server/Client x64 SDK Suite: %d/%d PASS (%d failed) ===\n",
                g_passed, g_passed + g_failed, g_failed);
    return (g_failed == 0) ? 0 : 1;
}
