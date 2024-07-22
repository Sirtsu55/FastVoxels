#include "DXRay/Device.h"

namespace DXR
{
    UINT64 Device::GetRequiredScratchBufferSize(std::vector<AccelerationStructureDesc>& descs)
    {
        UINT64 size = 0;

        for (auto& desc : descs)
        {
            size += desc.GetScratchBufferSize();
            // Always align the size to the required alignment
            size = DXR_ALIGN(size, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
        }

        return size;
    }

    ComPtr<DMA::Allocation> Device::AllocateAccelerationStructure(AccelerationStructureDesc& desc)
    {
        if (desc.Geometries.size() > 0 || desc.pGeometries.size() > 0)
            return InternalAllocateBottomAccelerationStructure(desc);
        else if (desc.vpInstanceDescs != 0)
            return InternalAllocateTopAccelerationStructure(desc);
        else
            DXR_THROW_FAILED(E_INVALIDARG);

        return nullptr;
    }

    ComPtr<DMA::Allocation> Device::InternalAllocateBottomAccelerationStructure(AccelerationStructureDesc& desc)
    {
        // Check if we have all required data for the function
        DXR_ASSERT(desc.pGeometries.size() > 0 || desc.Geometries.size() > 0,
                   "No geometries provided for bottom level acceleration structure");

        // Check what kind of geometry we have
        bool ppGeoms = desc.pGeometries.size() > 0;

        // Reference to the inputs for shorter code
        auto& inputs = desc.BuildDesc.Inputs;

        // Fill in the inputs
        inputs.DescsLayout = ppGeoms ? D3D12_ELEMENTS_LAYOUT_ARRAY_OF_POINTERS : D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = desc.Flags;
        inputs.InstanceDescs = 0;
        inputs.NumDescs = static_cast<UINT>(ppGeoms ? desc.pGeometries.size() : desc.Geometries.size());
        if (ppGeoms)
            inputs.ppGeometryDescs = desc.pGeometries.data();
        else
            inputs.pGeometryDescs = desc.Geometries.data();

        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

        // Query the prebuild info
        mDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &desc.PrebuildInfo);

        // Allocate the buffer
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
            desc.PrebuildInfo.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE);

        auto outAccel = AllocateResource(resDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        desc.BuildDesc.DestAccelerationStructureData = outAccel->GetResource()->GetGPUVirtualAddress();

        return outAccel;
    }

    ComPtr<DMA::Allocation> Device::InternalAllocateTopAccelerationStructure(AccelerationStructureDesc& desc)
    {
        // desc.vpInstanceDescs can be set at a later stage, but for now we require it to be set, to avoid unwanted
        // errors
        DXR_ASSERT(desc.vpInstanceDescs != 0, "No virtual address provided for top level acceleration structure");
        DXR_ASSERT(desc.NumInstanceDescs > 0, "Top level acceleration structure has no instances");

        // Reference to the inputs for shorter code
        auto& inputs = desc.BuildDesc.Inputs;

        // Fill in the inputs
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = desc.Flags;
        inputs.InstanceDescs = desc.vpInstanceDescs;
        inputs.NumDescs = desc.NumInstanceDescs;
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

        // Query the prebuild info
        mDevice->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &desc.PrebuildInfo);

        // Allocate the buffer
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
            desc.PrebuildInfo.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE);

        auto outAccel = AllocateResource(resDesc, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

        desc.BuildDesc.DestAccelerationStructureData = outAccel->GetResource()->GetGPUVirtualAddress();

        return outAccel;
    }

    void Device::BuildAccelerationStructure(AccelerationStructureDesc& desc,
                                            ComPtr<ID3D12GraphicsCommandList4>& cmdList)
    {
        DXR_ASSERT(desc.HasBeenAllocated(), "Acceleration structure has not been allocated");

        // TODO: Retrieve the postbuild info from the acceleration structure

        cmdList->BuildRaytracingAccelerationStructure(&desc.BuildDesc, 0, nullptr);
    }

    void Device::AssignScratchBuffer(std::vector<AccelerationStructureDesc>& descs, ComPtr<DMA::Allocation>& alloc)
    {
        UINT64 offset = 0;
        UINT64 maxSize = alloc->GetSize();
        D3D12_GPU_VIRTUAL_ADDRESS baseAddress = alloc->GetResource()->GetGPUVirtualAddress();

        for (auto& desc : descs)
        {
            desc.BuildDesc.ScratchAccelerationStructureData = baseAddress + offset;
            offset += desc.GetScratchBufferSize();
            offset = DXR_ALIGN(offset, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

            if (offset > maxSize)
                DXR_ASSERT(false, "Scratch buffer too small");
        }
    }

    ComPtr<DMA::Allocation> Device::AllocateAndAssignScratchBuffer(std::vector<AccelerationStructureDesc>& descs)
    {
        UINT64 size = GetRequiredScratchBufferSize(descs);

        ComPtr<DMA::Allocation> scratchBuffer = AllocateScratchBuffer(size);

        AssignScratchBuffer(descs, scratchBuffer);

        return scratchBuffer;
    }

    ComPtr<DMA::Allocation> Device::AllocateScratchBuffer(UINT64 size)
    {
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(size, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        return AllocateResource(resDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
    }

    ComPtr<DMA::Allocation> Device::AllocateInstanceBuffer(UINT64 numInstances, D3D12_HEAP_TYPE heapType)
    {
        D3D12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(
            numInstances * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_NONE);

        return AllocateResource(resDesc, D3D12_RESOURCE_STATE_COMMON, heapType);
    }

} // namespace DXR
