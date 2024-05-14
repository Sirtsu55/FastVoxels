#pragma once

#include "DXRay/Common.h"

namespace DXR
{
    /// @brief A description of an acceleration structure. Top or bottom.
    struct AccelerationStructureDesc
    {
    public: // Methods
        /// @brief Get the prebuild info for the acceleration structure.
        const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& GetPrebuildInfo() const { return PrebuildInfo; }

        /// @brief Get the build description for the acceleration structure.
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& GetBuildDesc() { return BuildDesc; }

        /// @brief Get the size of the scratch buffer needed to build the acceleration structure.
        UINT64 GetScratchBufferSize() const { return PrebuildInfo.ScratchDataSizeInBytes; }

        /// @brief Get the type of the acceleration structure.
        /// @return The type of the acceleration structure that will be built.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE GetType() const { return BuildDesc.Inputs.Type; }

        /// @brief Set the scratch buffer for the acceleration structure.
        void SetScratchBuffer(D3D12_GPU_VIRTUAL_ADDRESS scratchBuffer)
        {
            BuildDesc.ScratchAccelerationStructureData = scratchBuffer;
        }

        /// @brief Check if the acceleration structure has been allocated.
        bool HasBeenAllocated() const { return BuildDesc.DestAccelerationStructureData != 0; }

    public: // Members
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@ Common Acceleration Structure Info @@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// @brief TLAS & BLAS; The flags to use when building the acceleration structure.
        /// These must be set before calling AllocateAccelerationStructure
        /// because they are used to compute the prebuild info.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS Flags =
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;

        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@ Bottom Level Acceleration Structure @@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// @brief BLAS ONLY; The geometry to create the acceleration structure for.
        /// This or pGeometries must be set for BLAS building. No need to fill both.
        /// The pointers in this vector must be valid until the acceleration structure is built.
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> Geometries = {};

        /// @brief BLAS ONLY; The geometry to create the acceleration structure for, expressed as pointers.
        /// This or Geometries must be set for BLAS building. No need to fill both.
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC*> pGeometries = {};

        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@ Top Level Acceleration Structure @@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// @brief TLAS ONLY; The address of the instance descriptions that lie in GPU memory. This assumes that the
        /// address points to an array of D3D12_RAYTRACING_INSTANCE_DESC structures and not an array of pointers to
        /// D3D12_RAYTRACING_INSTANCE_DESC structures.
        /// For TLAS builds, this must be set and the memory must be valid when building the acceleration structure.
        /// @note This is the GPU virtual address, hence the "vp" prefix for "virtual pointer".
        D3D12_GPU_VIRTUAL_ADDRESS vpInstanceDescs = 0;

        /// @brief TLAS ONLY; The number of instance descriptions.
        /// For TLAS builds, this must be set and the memory must be valid when building the acceleration structure.
        UINT64 NumInstanceDescs = 0;

    private: // Members, These are not meant to be filled by the user, thus private, but they can be read by the user
        /// @brief The prebuild info for the acceleration structure.
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};

        /// @brief The acceleration structure build information.
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = {};

        friend class Device;
    };

} // namespace DXR
