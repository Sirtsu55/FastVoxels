#pragma once

#include "DXRay/Common.h"

namespace DXR
{
    /// @brief The type of shader in the shader table.
    enum class ShaderType
    {
        /// @brief A ray generation shader.
        RayGen,
        /// @brief A miss shader.
        Miss,
        /// @brief A hit group.
        HitGroup,
        /// @brief A callable shader.
        Callable
    };

    struct ShaderTable
    {
    public: // Methods
        /// @brief Add a shader to the shader table.
        /// @param name The unique name of the shader. In case it is a HitGroup shader, the name should be the name of
        /// the hit group. When building the shader table, this name will be used to get the shader identifier from the
        /// pipeline.
        void AddShader(const std::wstring& name, ShaderType type)
        {
            DXR_ASSERT(mShaders.find(name) == mShaders.end(), "Shader already exists in shader table.");
            // Shader will have a entry assigned when the shader table is built.
            mShaders.emplace(name, ShaderTableEntry {0, type});

            switch (type)
            {
            case ShaderType::RayGen: mNumRayGenShaders++; break;
            case ShaderType::Miss: mNumMissShaders++; break;
            case ShaderType::HitGroup: mNumHitGroupShaders++; break;
            case ShaderType::Callable: mNumCallableShaders++; break;
            default: DXR_ASSERT(false, "Invalid shader type."); break;
            }
        }

        /// @brief Reserves space for new shaders so that new allocations arent made in the hash map when adding new
        /// shaders.
        void ReserveHashmapSpace(UINT32 num) { mShaders.reserve(num); }

        /// @brief Reserve space for new shaders so that any when updating the shader table with new shaders, the
        /// shader table does not need to be reallocated.
        /// @param num The number of shaders to reserve space for.
        /// @param type The type of shader to reserve space for.
        /// @note If called multiple times, it will increment the number of shaders to reserve space for.
        void ReserveSpaceForShaders(UINT32 num, ShaderType type)
        {
            switch (type)
            {
            case ShaderType::RayGen: mNumRayGenShaders += num; break;
            case ShaderType::Miss: mNumMissShaders += num; break;
            case ShaderType::HitGroup: mNumHitGroupShaders += num; break;
            case ShaderType::Callable: mNumCallableShaders += num; break;
            default: DXR_ASSERT(false, "Invalid shader type."); break;
            }
        }

        /// @brief Get the D3D12_DISPATCH_RAYS_DESC for the shader table.
        /// @param rgen The index of the ray gen shader in the shader table
        /// @param width The width of the dispatch.
        /// @param height The height of the dispatch.
        /// @param depth The depth of the dispatch.
        /// @return The D3D12_DISPATCH_RAYS_DESC for the shader table.
        D3D12_DISPATCH_RAYS_DESC GetRaysDesc(UINT32 rgen, UINT32 width, UINT32 height, UINT32 depth = 1)
        {
            mDispatchDesc.Height = height;
            mDispatchDesc.Width = width;
            mDispatchDesc.Depth = depth;

            mDispatchDesc.RayGenerationShaderRecord.StartAddress =
                mShaderTableGPUAddress + (rgen * mRayGenShaderRecordSize);

            return mDispatchDesc;
        }

        /// @brief Get the allocation for the shader table.
        /// @return The allocation for the shader table.
        ComPtr<DMA::Allocation> GetShaderTableAllocation() const { return mShaderTable; }

        /// @brief Set the size of the shader records, including the shader identifier, which is
        /// D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES.
        /// @param size The size of the shader records.
        void SetShaderRecordSize(UINT64 size, ShaderType type)
        {
            DXR_ASSERT(mShaderTable != nullptr, "Shader table is already built. Cannot modify shader record size.");
            DXR_ASSERT(size <= D3D12_RAYTRACING_MAX_SHADER_RECORD_STRIDE, "Shader record size is too large.");

            switch (type)
            {
            case ShaderType::RayGen:
                // Ray gen shaders are aligned to D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT, because there are only
                // one per
                // shader table.
                mRayGenShaderRecordSize = DXR_ALIGN(size, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
                break;
            case ShaderType::Miss:
                mMissShaderRecordSize = DXR_ALIGN(size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
                break;
            case ShaderType::HitGroup:
                mHitGroupRecordSize = DXR_ALIGN(size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
                break;
            case ShaderType::Callable:
                mCallableRecordSize = DXR_ALIGN(size, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
                break;

            default: DXR_ASSERT(false, "Invalid shader type."); break;
            }
        }

        /// @brief Set the data for local root arguments for a shader in the shader table.
        /// @param name The name of the shader to set the local root arguments for.
        /// @param data The data to set for the local root arguments.
        /// @param size The size of the data to set.
        /// @param offset The offset to set the data at.
        void SetShaderRecordData(const std::wstring& name, const void* data, UINT32 size, UINT32 offset = 0)
        {
            DXR_ASSERT(mShaderTable != nullptr, "Shader table must be built before setting shader record data.");

            auto shader = mShaders.find(name);

            DXR_ASSERT(shader != mShaders.end(), "Shader does not exist in shader table.");

            auto& entry = shader->second;

            CHAR* ptr = nullptr;

            switch (entry.Type)
            {
            case ShaderType::RayGen:
            {
                ptr = mRayGenStartPtr + (entry.Index * mRayGenShaderRecordSize);
                break;
            }
            case ShaderType::Miss:
            {
                ptr = mMissStartPtr + (entry.Index * mMissShaderRecordSize);
                break;
            }
            case ShaderType::HitGroup:
            {
                ptr = mHitGroupStartPtr + (entry.Index * mHitGroupRecordSize);
                break;
            }
            case ShaderType::Callable:
            {
                ptr = mCallableStartPtr + (entry.Index * mCallableRecordSize);
                break;
            }
            default: DXR_ASSERT(false, "Invalid shader type."); break;
            }

            // Add the shader identifier to the start of the shader record, because that should always be there.
            ptr += D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES + offset;
            memcpy(ptr, data, size);
        }

    private: // Private Structs
        struct ShaderTableEntry
        {
            /// @brief The index of the shader in the shader table.
            UINT32 Index;

            /// @brief The type of shader.
            ShaderType Type;
        };

    private: // Members
        // The buffer resource for the shader table.
        ComPtr<DMA::Allocation> mShaderTable;
        UINT64 mShaderTableGPUAddress = 0;

        // The pointer to the start of each shader table.
        CHAR* mRayGenStartPtr = nullptr;
        CHAR* mMissStartPtr = nullptr;
        CHAR* mHitGroupStartPtr = nullptr;
        CHAR* mCallableStartPtr = nullptr;

        // The size of the records for each shader type.
        UINT64 mRayGenShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        UINT64 mMissShaderRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        UINT64 mHitGroupRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
        UINT64 mCallableRecordSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

        // The names of the shaders in the shader table.
        // Map 1-1 with the shader table.

        std::unordered_map<std::wstring, ShaderTableEntry> mShaders;

        // Whether or not the shader table needs to be rebuilt.
        bool mNeedsReallocation = true;
        bool mNewShadersAdded = false;

        // How many shaders have been built into the shader table.

        UINT32 mRayGenShadersBuilt = 0;
        UINT32 mMissShadersBuilt = 0;
        UINT32 mHitGroupShadersBuilt = 0;
        UINT32 mCallableShadersBuilt = 0;

        // How much shaders are in the shader table.

        UINT32 mNumRayGenShaders = 0;
        UINT32 mNumMissShaders = 0;
        UINT32 mNumHitGroupShaders = 0;
        UINT32 mNumCallableShaders = 0;

        D3D12_DISPATCH_RAYS_DESC mDispatchDesc = {};

        friend class Device;
    };

} // namespace DXR
