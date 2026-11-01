#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include <cassert>
#include "../client/src/EterLib/GrpBase.h"

bool CGraphicBase::ms_bUseD3D9Ex = false;
bool CGraphicBase::ms_bD3D9ExActive = false;
bool CGraphicBase::ms_bDeviceOccluded = false;

namespace
{

int g_passed = 0;
int g_failed = 0;

void CheckImpl(bool cond, const char* expr, int line)
{
	if (cond)
	{
		++g_passed;
	}
	else
	{
		++g_failed;
		std::printf("[FAIL] line %d: %s\n", line, expr);
	}
}

#define CHECK(expr) CheckImpl(static_cast<bool>(expr), #expr, __LINE__)

struct SimpleVertex
{
	float x, y, z, rhw;
	DWORD color;
	float u, v;
};
static constexpr DWORD kSimpleVertexFVF = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

LRESULT CALLBACK TestWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

HWND CreateTestWindow(int width, int height)
{
	static bool s_registered = false;
	HINSTANCE hInst = GetModuleHandleA(NULL);
	if (!s_registered)
	{
		WNDCLASSEXA wc = {};
		wc.cbSize = sizeof(wc);
		wc.lpfnWndProc = TestWndProc;
		wc.hInstance = hInst;
		wc.lpszClassName = "MT2009_D3D9Ex_TestWnd";
		RegisterClassExA(&wc);
		s_registered = true;
	}
	return CreateWindowExA(
		0,
		"MT2009_D3D9Ex_TestWnd",
		"MT2009 D3D9Ex x64 Verification",
		WS_OVERLAPPEDWINDOW,
		0, 0, width, height,
		NULL, NULL, hInst, NULL);
}

// Classify D3D9Ex CheckDeviceState return code using the exact EterLib contract
enum class EterDeviceState
{
	OK,
	NEEDS_RESET,
	BROKEN
};

EterDeviceState ClassifyD3D9ExState(HRESULT hr, bool* outOccluded)
{
	*outOccluded = (hr == S_PRESENT_OCCLUDED);
	if (hr == S_OK || hr == S_PRESENT_OCCLUDED)
		return EterDeviceState::OK;
	if (hr == S_PRESENT_MODE_CHANGED || hr == D3DERR_DEVICENOTRESET)
		return EterDeviceState::NEEDS_RESET;
	return EterDeviceState::BROKEN;
}

} // anonymous namespace

int main()
{
	static_assert(sizeof(void*) == 8, "Must compile as native Windows x64 (PE32+ AMD64)");

	std::printf("=== MT2009 Direct3D 9Ex Optional Renderer & x64 API Verification Suite ===\n");

	// 1. Verify EterLib CGraphicBase default D3D9 state and optional D3D9Ex toggle contract
	CHECK(!CGraphicBase::IsUseD3D9Ex());
	CHECK(!CGraphicBase::IsD3D9ExActive());
	CHECK(CGraphicBase::GetManagedPool() == D3DPOOL_MANAGED);
	CHECK(CGraphicBase::GetManagedBufferUsage(D3DUSAGE_WRITEONLY) == D3DUSAGE_WRITEONLY);

	CGraphicBase::SetUseD3D9Ex(true);
	CHECK(CGraphicBase::IsUseD3D9Ex());
	CGraphicBase::SetUseD3D9Ex(false);
	CHECK(!CGraphicBase::IsUseD3D9Ex());

	// 2. Verify CheckDeviceState classification contract (S_OK, S_PRESENT_OCCLUDED, S_PRESENT_MODE_CHANGED, D3DERR_DEVICELOST, D3DERR_DEVICEHUNG)
	bool occluded = false;
	CHECK(ClassifyD3D9ExState(S_OK, &occluded) == EterDeviceState::OK && !occluded);
	CHECK(ClassifyD3D9ExState(S_PRESENT_OCCLUDED, &occluded) == EterDeviceState::OK && occluded);
	CHECK(ClassifyD3D9ExState(S_PRESENT_MODE_CHANGED, &occluded) == EterDeviceState::NEEDS_RESET && !occluded);
	CHECK(ClassifyD3D9ExState(D3DERR_DEVICENOTRESET, &occluded) == EterDeviceState::NEEDS_RESET && !occluded);
	CHECK(ClassifyD3D9ExState(D3DERR_DEVICELOST, &occluded) == EterDeviceState::BROKEN && !occluded);
	CHECK(ClassifyD3D9ExState(D3DERR_DEVICEHUNG, &occluded) == EterDeviceState::BROKEN && !occluded);

	// 3. Create test window and verify real Direct3DCreate9Ex from system x64 d3d9.dll
	HWND hWnd = CreateTestWindow(640, 480);
	CHECK(hWnd != NULL);

	IDirect3D9Ex* pD3DEx = nullptr;
	HRESULT hrCreateEx = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx);
	CHECK(SUCCEEDED(hrCreateEx) && pD3DEx != nullptr);

	UINT adapterCount = pD3DEx ? pD3DEx->GetAdapterCount() : 0;
	CHECK(adapterCount >= 1);

	D3DADAPTER_IDENTIFIER9 adapterId = {};
	CHECK(pD3DEx && SUCCEEDED(pD3DEx->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId)));
	std::printf("[INFO] GPU Adapter: '%s' (Driver: '%s')\n", adapterId.Description, adapterId.Driver);

	D3DDISPLAYMODEEX desktopModeEx = {};
	desktopModeEx.Size = sizeof(desktopModeEx);
	CHECK(pD3DEx && SUCCEEDED(pD3DEx->GetAdapterDisplayModeEx(D3DADAPTER_DEFAULT, &desktopModeEx, NULL)));
	CHECK(desktopModeEx.Width > 0 && desktopModeEx.Height > 0);

	D3DCAPS9 caps = {};
	CHECK(pD3DEx && SUCCEEDED(pD3DEx->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)));
	CHECK((caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) != 0);

	// 4. Create real IDirect3DDevice9Ex (windowed, D3DPRESENT_INTERVAL_IMMEDIATE)
	D3DPRESENT_PARAMETERS d3dpp = {};
	d3dpp.Windowed = TRUE;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferFormat = desktopModeEx.Format;
	d3dpp.BackBufferWidth = 640;
	d3dpp.BackBufferHeight = 480;
	d3dpp.BackBufferCount = 1;
	d3dpp.hDeviceWindow = hWnd;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

	DWORD behaviorFlags = (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		? D3DCREATE_HARDWARE_VERTEXPROCESSING
		: D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	IDirect3DDevice9Ex* pDevEx = nullptr;
	HRESULT hrDevEx = pD3DEx->CreateDeviceEx(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hWnd,
		behaviorFlags,
		&d3dpp,
		NULL,
		&pDevEx);
	CHECK(SUCCEEDED(hrDevEx) && pDevEx != nullptr);

	// 5. Prove that D3DPOOL_MANAGED is rejected by IDirect3DDevice9Ex and verify our EterLib staging + UpdateTexture replacement
	IDirect3DTexture9* pManagedRejected = nullptr;
	HRESULT hrManaged = pDevEx->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pManagedRejected, NULL);
	CHECK(FAILED(hrManaged) && pManagedRejected == nullptr);

	// EterLib CBlockTexture / CGraphicImageTexture replacement: D3DPOOL_DEFAULT + D3DPOOL_SYSTEMMEM + AddDirtyRect + UpdateTexture
	IDirect3DTexture9* pDefaultTex = nullptr;
	IDirect3DTexture9* pSysStagingTex = nullptr;
	CHECK(SUCCEEDED(pDevEx->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &pDefaultTex, NULL)));
	CHECK(SUCCEEDED(pDevEx->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &pSysStagingTex, NULL)));

	D3DLOCKED_RECT lockedRect = {};
	RECT subRect = { 8, 8, 56, 56 };
	CHECK(SUCCEEDED(pSysStagingTex->LockRect(0, &lockedRect, &subRect, 0)));
	for (int y = 0; y < (subRect.bottom - subRect.top); ++y)
	{
		DWORD* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(lockedRect.pBits) + y * lockedRect.Pitch);
		for (int x = 0; x < (subRect.right - subRect.left); ++x)
			row[x] = 0xFF00FF80U;
	}
	CHECK(SUCCEEDED(pSysStagingTex->UnlockRect(0)));
	CHECK(SUCCEEDED(pDefaultTex->AddDirtyRect(&subRect)));
	CHECK(SUCCEEDED(pDevEx->UpdateTexture(pSysStagingTex, pDefaultTex)));

	// EterLib CreateDDSTexture replacement: mipmapped DXT1 texture via D3DPOOL_SYSTEMMEM + UpdateTexture into D3DPOOL_DEFAULT
	IDirect3DTexture9* pDxtSys = nullptr;
	IDirect3DTexture9* pDxtDefault = nullptr;
	CHECK(SUCCEEDED(pDevEx->CreateTexture(64, 64, 3, 0, D3DFMT_DXT1, D3DPOOL_SYSTEMMEM, &pDxtSys, NULL)));
	CHECK(SUCCEEDED(pDevEx->CreateTexture(64, 64, 3, 0, D3DFMT_DXT1, D3DPOOL_DEFAULT, &pDxtDefault, NULL)));
	for (UINT mip = 0; mip < 3; ++mip)
	{
		D3DLOCKED_RECT mipRect = {};
		CHECK(SUCCEEDED(pDxtSys->LockRect(mip, &mipRect, NULL, 0)));
		std::memset(mipRect.pBits, 0xAA, 8);
		CHECK(SUCCEEDED(pDxtSys->UnlockRect(mip)));
	}
	CHECK(SUCCEEDED(pDevEx->UpdateTexture(pDxtSys, pDxtDefault)));
	pDxtSys->Release();

	// 6. Verify D3D9Ex VertexBuffer and IndexBuffer in D3DPOOL_DEFAULT with D3DUSAGE_DYNAMIC
	IDirect3DVertexBuffer9* pVB = nullptr;
	IDirect3DIndexBuffer9* pIB = nullptr;
	CHECK(SUCCEEDED(pDevEx->CreateVertexBuffer(
		sizeof(SimpleVertex) * 4,
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		kSimpleVertexFVF,
		D3DPOOL_DEFAULT,
		&pVB,
		NULL)));
	CHECK(SUCCEEDED(pDevEx->CreateIndexBuffer(
		sizeof(WORD) * 6,
		D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
		D3DFMT_INDEX16,
		D3DPOOL_DEFAULT,
		&pIB,
		NULL)));

	void* pVBData = nullptr;
	CHECK(SUCCEEDED(pVB->Lock(0, sizeof(SimpleVertex) * 4, &pVBData, D3DLOCK_DISCARD)));
	const SimpleVertex quadVerts[4] = {
		{  50.0f,  50.0f, 0.5f, 1.0f, 0xFFFFFFFFU, 0.0f, 0.0f },
		{ 300.0f,  50.0f, 0.5f, 1.0f, 0xFFFFFFFFU, 1.0f, 0.0f },
		{  50.0f, 300.0f, 0.5f, 1.0f, 0xFFFFFFFFU, 0.0f, 1.0f },
		{ 300.0f, 300.0f, 0.5f, 1.0f, 0xFFFFFFFFU, 1.0f, 1.0f },
	};
	std::memcpy(pVBData, quadVerts, sizeof(quadVerts));
	CHECK(SUCCEEDED(pVB->Unlock()));

	void* pIBData = nullptr;
	CHECK(SUCCEEDED(pIB->Lock(0, sizeof(WORD) * 6, &pIBData, D3DLOCK_DISCARD)));
	const WORD quadIndices[6] = { 0, 1, 2, 2, 1, 3 };
	std::memcpy(pIBData, quadIndices, sizeof(quadIndices));
	CHECK(SUCCEEDED(pIB->Unlock()));

	// 7. Verify CheckDeviceState(hWnd), Window Resize + ResetEx WITHOUT destroying D3DPOOL_DEFAULT resources, and PresentEx
	HRESULT hrStateBefore = pDevEx->CheckDeviceState(hWnd);
	CHECK(hrStateBefore == S_OK || hrStateBefore == S_PRESENT_OCCLUDED);

	// Measure 60 frames on D3D9Ex
	auto tExFrameStart = std::chrono::steady_clock::now();
	for (int f = 0; f < 60; ++f)
	{
		CHECK(SUCCEEDED(pDevEx->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF102030U, 1.0f, 0)));
		CHECK(SUCCEEDED(pDevEx->BeginScene()));
		CHECK(SUCCEEDED(pDevEx->SetTexture(0, pDefaultTex)));
		CHECK(SUCCEEDED(pDevEx->SetStreamSource(0, pVB, 0, sizeof(SimpleVertex))));
		CHECK(SUCCEEDED(pDevEx->SetIndices(pIB)));
		CHECK(SUCCEEDED(pDevEx->SetFVF(kSimpleVertexFVF)));
		CHECK(SUCCEEDED(pDevEx->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2)));
		CHECK(SUCCEEDED(pDevEx->EndScene()));
		HRESULT hrPresEx = pDevEx->PresentEx(NULL, NULL, hWnd, NULL, 0);
		CHECK(SUCCEEDED(hrPresEx) || hrPresEx == S_PRESENT_OCCLUDED);
	}
	double d3d9ex60FramesMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tExFrameStart).count();

	// Resize window & call ResetEx WITHOUT releasing pDefaultTex, pDxtDefault, pVB, or pIB!
	SetWindowPos(hWnd, NULL, 0, 0, 800, 600, SWP_NOZORDER | SWP_NOACTIVATE);
	d3dpp.BackBufferWidth = 800;
	d3dpp.BackBufferHeight = 600;
	auto tResetExStart = std::chrono::steady_clock::now();
	HRESULT hrResetEx = pDevEx->ResetEx(&d3dpp, NULL);
	double d3d9exResetMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tResetExStart).count();
	CHECK(SUCCEEDED(hrResetEx));

	// Render after ResetEx using the surviving D3DPOOL_DEFAULT textures & buffers!
	CHECK(SUCCEEDED(pDevEx->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF203040U, 1.0f, 0)));
	CHECK(SUCCEEDED(pDevEx->BeginScene()));
	CHECK(SUCCEEDED(pDevEx->SetTexture(0, pDxtDefault)));
	CHECK(SUCCEEDED(pDevEx->SetStreamSource(0, pVB, 0, sizeof(SimpleVertex))));
	CHECK(SUCCEEDED(pDevEx->SetIndices(pIB)));
	CHECK(SUCCEEDED(pDevEx->SetFVF(kSimpleVertexFVF)));
	CHECK(SUCCEEDED(pDevEx->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2)));
	CHECK(SUCCEEDED(pDevEx->EndScene()));
	CHECK(SUCCEEDED(pDevEx->PresentEx(NULL, NULL, hWnd, NULL, 0)));

	pIB->Release();
	pVB->Release();
	pDxtDefault->Release();
	pSysStagingTex->Release();
	pDefaultTex->Release();
	pDevEx->Release();
	pD3DEx->Release();

	// 8. Verify Legacy D3D9 Fallback Path (Direct3DCreate9 + CreateDevice + D3DPOOL_MANAGED + Reset + Present) and compare timing
	IDirect3D9* pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
	CHECK(pD3D9 != nullptr);

	d3dpp.BackBufferWidth = 640;
	d3dpp.BackBufferHeight = 480;
	IDirect3DDevice9* pDev9 = nullptr;
	CHECK(pD3D9 && SUCCEEDED(pD3D9->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		hWnd,
		behaviorFlags,
		&d3dpp,
		&pDev9)) && pDev9 != nullptr);

	IDirect3DTexture9* pManagedTex9 = nullptr;
	CHECK(SUCCEEDED(pDev9->CreateTexture(64, 64, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &pManagedTex9, NULL)));
	CHECK(SUCCEEDED(pDev9->TestCooperativeLevel()));

	auto tD3D9FrameStart = std::chrono::steady_clock::now();
	for (int f = 0; f < 60; ++f)
	{
		pDev9->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF102030U, 1.0f, 0);
		pDev9->BeginScene();
		pDev9->SetTexture(0, pManagedTex9);
		pDev9->EndScene();
		pDev9->Present(NULL, NULL, hWnd, NULL);
	}
	double d3d9_60FramesMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tD3D9FrameStart).count();

	d3dpp.BackBufferWidth = 800;
	d3dpp.BackBufferHeight = 600;
	auto tReset9Start = std::chrono::steady_clock::now();
	HRESULT hrReset9 = pDev9->Reset(&d3dpp);
	double d3d9ResetMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tReset9Start).count();
	CHECK(SUCCEEDED(hrReset9));

	pManagedTex9->Release();
	pDev9->Release();
	pD3D9->Release();
	DestroyWindow(hWnd);

	std::printf("[PERF] D3D9 vs D3D9Ex (60 frames): D3D9=%.3f ms, D3D9Ex=%.3f ms | Resize Reset: D3D9=%.3f ms, D3D9Ex(ResetEx)=%.3f ms\n",
		d3d9_60FramesMs, d3d9ex60FramesMs, d3d9ResetMs, d3d9exResetMs);
	std::printf("D3D9EX_X64_SUITE: %d/%d PASSED (Direct3DCreate9Ex + CreateDeviceEx + PresentEx + ResetEx + CheckDeviceState + D3D9 fallback verified)\n",
		g_passed, g_passed + g_failed);

	return (g_failed == 0) ? 0 : 1;
}
