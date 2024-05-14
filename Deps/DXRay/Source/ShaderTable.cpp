#include "DXRay/ShaderTable.h"

namespace DXR
{
    void Device::CreateShaderTable(ShaderTable& table, D3D12_HEAP_TYPE heap, ComPtr<ID3D12StateObject>& pipeline)
    {
        DXR_ASSERT(heap != D3D12_HEAP_TYPE_DEFAULT, "Shader table must be in heap that is CPU accessible");

        UINT64 numRgen = table.mNumRayGenShaders;
        UINT64 numMiss = table.mNumMissShaders;
        UINT64 numHitGroup = table.mNumHitGroupShaders;
        UINT64 numCallable = table.mNumCallableShaders;

        UINT64 rgenAlignedSize =
            DXR_ALIGN(numRgen * table.mRayGenShaderRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        UINT64 missAlignedSize =
            DXR_ALIGN(numMiss * table.mMissShaderRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        UINT64 hitGroupAlignedSize =
            DXR_ALIGN(numHitGroup * table.mHitGroupRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        UINT64 callableAlignedSize =
            DXR_ALIGN(numCallable * table.mCallableRecordSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

        auto tableBufDesc = CD3DX12_RESOURCE_DESC::Buffer(rgenAlignedSize + missAlignedSize + hitGroupAlignedSize +
                                                          callableAlignedSize);

        table.mShaderTable = AllocateResource(tableBufDesc, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, heap,
                                              DMA::ALLOCATION_FLAG_NONE, D3D12_HEAP_FLAG_NONE);

        CHAR* pData = reinterpret_cast<CHAR*>(MapAllocationForWrite(table.mShaderTable));

        table.mRayGenStartPtr = pData;
        pData += rgenAlignedSize;
        table.mMissStartPtr = pData;
        pData += missAlignedSize;
        table.mHitGroupStartPtr = pData;
        pData += hitGroupAlignedSize;
        table.mCallableStartPtr = pData;

        // Now that we have the pointers, we can fill in the shader table with identifiers

        ComPtr<ID3D12StateObjectProperties> stateObjectProps = nullptr;
        DXR_THROW_FAILED(pipeline.As(&stateObjectProps));

        for (auto& [name, entry] : table.mShaders)
        {

            switch (entry.Type)
            {
            case ShaderType::RayGen:
            {
                void* pShaderId = stateObjectProps->GetShaderIdentifier(name.c_str());
                memcpy(table.mRayGenStartPtr + (table.mRayGenShadersBuilt * table.mRayGenShaderRecordSize), pShaderId,
                       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                entry.Index = table.mRayGenShadersBuilt++; // Assign the index to the shader and increment the counter
                break;
            }
            case ShaderType::Miss:
            {
                void* pShaderId = stateObjectProps->GetShaderIdentifier(name.c_str());
                memcpy(table.mMissStartPtr + (table.mMissShadersBuilt * table.mMissShaderRecordSize), pShaderId,
                       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                entry.Index = table.mMissShadersBuilt++; // Assign the index to the shader and increment the counter
                break;
            }
            case ShaderType::HitGroup:
            {
                void* pShaderId = stateObjectProps->GetShaderIdentifier(name.c_str());
                memcpy(table.mHitGroupStartPtr + (table.mHitGroupShadersBuilt * table.mHitGroupRecordSize), pShaderId,
                       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                entry.Index = table.mHitGroupShadersBuilt++; // Assign the index to the shader and increment the counter
                break;
            }
            case ShaderType::Callable:
            {
                void* pShaderId = stateObjectProps->GetShaderIdentifier(name.c_str());
                memcpy(table.mCallableStartPtr + (table.mCallableShadersBuilt * table.mCallableRecordSize), pShaderId,
                       D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
                entry.Index = table.mCallableShadersBuilt++; // Assign the index to the shader and increment the counter
                break;
            }
            }
        }

        table.mShaderTableGPUAddress = table.mShaderTable->GetResource()->GetGPUVirtualAddress();

        if (table.mNumRayGenShaders > 0)
        {
            table.mDispatchDesc.RayGenerationShaderRecord.SizeInBytes = table.mRayGenShaderRecordSize;
        }
        if (table.mNumMissShaders > 0)
        {
            table.mDispatchDesc.MissShaderTable.StartAddress = table.mShaderTableGPUAddress + rgenAlignedSize;
            table.mDispatchDesc.MissShaderTable.SizeInBytes = table.mMissShaderRecordSize * table.mNumMissShaders;
            table.mDispatchDesc.MissShaderTable.StrideInBytes = table.mMissShaderRecordSize;
        }
        if (table.mNumHitGroupShaders > 0)
        {
            table.mDispatchDesc.HitGroupTable.StartAddress =
                table.mShaderTableGPUAddress + rgenAlignedSize + missAlignedSize;
            table.mDispatchDesc.HitGroupTable.SizeInBytes = table.mHitGroupRecordSize * table.mNumHitGroupShaders;
            table.mDispatchDesc.HitGroupTable.StrideInBytes = table.mHitGroupRecordSize;
        }
        if (table.mNumCallableShaders > 0)
        {
            table.mDispatchDesc.CallableShaderTable.StartAddress =
                table.mShaderTableGPUAddress + rgenAlignedSize + missAlignedSize + hitGroupAlignedSize;
            table.mDispatchDesc.CallableShaderTable.SizeInBytes = table.mCallableRecordSize * table.mNumCallableShaders;
            table.mDispatchDesc.CallableShaderTable.StrideInBytes = table.mCallableRecordSize;
        }

        table.mNeedsReallocation = false;
        table.mNewShadersAdded = false;
    }

} // namespace DXR
