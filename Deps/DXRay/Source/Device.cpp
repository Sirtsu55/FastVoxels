#include "DXRay/Device.h"

namespace DXR
{
    Device::Device(ComPtr<DXRDevice> device, ComPtr<IDXGIAdapter> adapter, ComPtr<DMA::Allocator> allocator)
        : mDevice(device), mAdapter(adapter), mAllocator(allocator)
    {
        if (mAllocator == nullptr)
        {
            DMA::ALLOCATOR_DESC desc = {};
            desc.pAdapter = mAdapter.Get();
            desc.pDevice = mDevice.Get();
            desc.Flags = DMA::ALLOCATOR_FLAG_NONE;
            DXR_THROW_FAILED(DMA::CreateAllocator(&desc, &mAllocator));
        }
    }

    Device::~Device()
    {
    }

    void* Device::MapAllocationForWrite(ComPtr<DMA::Allocation>& res)
    {
        void* mapped;
        CD3DX12_RANGE readRange(0, 0);
        DXR_THROW_FAILED(res->GetResource()->Map(0, &readRange, &mapped));
        return mapped;
    }

    ComPtr<DMA::Allocation> Device::AllocateResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state,
                                                     D3D12_HEAP_TYPE heapType, DMA::ALLOCATION_FLAGS allocFlags,
                                                     D3D12_HEAP_FLAGS heapFlags)
    {
        ComPtr<DMA::Allocation> outAlloc = nullptr;

        DMA::ALLOCATION_DESC allocDesc = {};
        allocDesc.HeapType = heapType;
        allocDesc.Flags = allocFlags;
        allocDesc.ExtraHeapFlags = heapFlags;
        allocDesc.CustomPool = mPool; // if mPool is nullptr, the default pool will be used

        DXR_THROW_FAILED(mAllocator->CreateResource(&allocDesc, &desc, state, nullptr, &outAlloc, {}, nullptr));
        return outAlloc;
    }

} // namespace DXR
