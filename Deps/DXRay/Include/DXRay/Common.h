#pragma once

// Windows Header Files
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>

// DirectX Header Files
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>

// D3D12 Memory Allocator
#include <d3d12memalloc.h>

// STL Headers
#include <unordered_map>
#include <vector>

namespace DXR
{
    using namespace Microsoft::WRL;
}

// D3D12 Memory Allocator alias
namespace DMA = D3D12MA;

// Helper Defines
#define DXR_ALIGN(num, alignment) (((num) + alignment - 1) & ~(alignment - 1))

// Enable debugging by default in debug builds, disable in release builds unless explicitly enabled
#if defined(DXR_ENABLE_DEBUG) || _DEBUG
#define DXR_LOG_DEBUG(msg) OutputDebugStringA(msg)
#define DXR_ASSERT(cond, msg)                                                                                          \
    if (!(cond))                                                                                                       \
    {                                                                                                                  \
        DXR_LOG_DEBUG(msg);                                                                                            \
        __debugbreak();                                                                                                \
    }
#define DXR_THROW_FAILED(hresult)                                                                                      \
    if (FAILED(hresult))                                                                                               \
    {                                                                                                                  \
        __debugbreak();                                                                                                \
    }
#else
#define DXR_LOG_DEBUG(msg) ((void)0)
#define DXR_ASSERT(cond, msg) ((void)0)
#define DXR_THROW_FAILED(hresult) hresult
#endif
