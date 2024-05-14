#pragma once

#include "DXRay/Common.h"
#include "DXRay/AccelStruct.h"
#include "DXRay/ShaderTable.h"

// Namespace is too long to type out, so shorten it to DMA: |D|3D12 |M|emory |A|llocator

namespace DXR
{
    /// @brief A wrapper around the D3D12 to use DXR.
    class Device
    {
        /// @brief Alias for the D3D12 device type so it's easier to change later
        /// if new versions are released
        using DXRDevice = ID3D12Device7;
        using DXRAdapter = IDXGIAdapter;

    public:
        /// @brief Create a Device
        /// @param device The D3D12 device to use
        /// @param allocator The allocator to use for all allocations, if null a new allocator will be created
        /// @note General Tips:
        /// - To get complete error messages, enable the debug layer and the info queue, DXRay only throws exceptions
        /// based on HRESULTs
        /// - If you want to use a custom allocator, create it before creating the device
        /// - If you want to use a custom pool, set it after creating the device
        Device(ComPtr<DXRDevice> device, ComPtr<IDXGIAdapter> adapter, ComPtr<DMA::Allocator> allocator = nullptr);
        ~Device();

        // Delete copy/move constructors and assignment operators

        Device(Device const&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device const&) = delete;
        Device& operator=(Device&&) = delete;

    public:
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@ Getters & Setters @@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// @brief Get the D3D12 device
        /// @return The D3D12 device
        ComPtr<DXRDevice> GetD3D12Device() const { return mDevice; }

        /// @brief Get the allocator
        /// @return The allocator
        ComPtr<DMA::Allocator> GetAllocator() const { return mAllocator; }

        /// @brief Allocate a big scratch buffer that will be used for all acceleration structures
        /// This is the recommended function to call, because it will take into account the alignment requirements
        /// @param descs The descriptions of the acceleration structures which will be assigned a region of the scratch
        UINT64 GetRequiredScratchBufferSize(std::vector<AccelerationStructureDesc>& descs);

        /// @brief Get the pool
        /// @return The pool, or null if no pool is set
        DMA::Pool* GetPool() const { return mPool; }

        /// @brief Set the pool
        /// @param pool The pool to use for all allocations
        /// @note This will override the existing pool if one is already set
        void SetPool(DMA::Pool* pool) { mPool = pool; }

        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Utilities @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// @brief Allocate a resource
        /// @param desc The description of the resource
        /// @param state The initial state of the resource
        /// @param heapType The type of heap to use, default is DEFAULT (GPU only)
        /// @param allocFlags The allocation flags to use, default is NONE
        /// @param heapFlags The heap flags to use, default is NONE
        /// @return The new resource
        ComPtr<DMA::Allocation> AllocateResource(const D3D12_RESOURCE_DESC& desc, D3D12_RESOURCE_STATES state,
                                                 D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT,
                                                 DMA::ALLOCATION_FLAGS allocFlags = DMA::ALLOCATION_FLAG_NONE,
                                                 D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

        /// @brief Map a resource for only writing. It is always recommended to map persistently if the resource is
        /// located in CPU visible memory to avoid Map/Unmap overhead.
        /// @param resource The resource to map
        /// @return The mapped pointer
        void* MapAllocationForWrite(ComPtr<DMA::Allocation>& res);

        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@ Accel Struct @@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// @brief Allocate a new acceleration structure. When allocating a bottom level acceleration structure,
        /// the returned allocation's GPU virtual address is used for instance description acceleration structure field.
        /// @param desc The description of the acceleration structure. Will be modified and used when building the
        /// acceleration structure.
        /// @return The new acceleration structure.
        ComPtr<DMA::Allocation> AllocateAccelerationStructure(AccelerationStructureDesc& desc);

        /// @brief Build an acceleration structure
        /// @param desc The description of the acceleration structure to build
        /// @param cmdList The command list to use for building
        void BuildAccelerationStructure(AccelerationStructureDesc& desc, ComPtr<ID3D12GraphicsCommandList4>& cmdList);

        /// @brief Allocate a scratch buffer for building a bottom level acceleration structure. It will take into
        /// account the alignment requirements.
        /// @param desc The description of the acceleration structure which will be assigned a region of the scratch
        /// buffer
        /// @param alloc The scratch buffer to use. This allocation should be of the size returned by
        /// GetRequiredScratchBufferSize(...) to ensure that it is large enough.
        void AssignScratchBuffer(std::vector<AccelerationStructureDesc>& descs, ComPtr<DMA::Allocation>& alloc);

        /// @brief Allocate a big scratch buffer that will be used for all acceleration structures and assign regions
        /// to each acceleration structure. This is a convenience function that calls GetRequiredScratchBufferSize(...),
        /// AllocateScratchBuffer(...) and AssignScratchBuffer(...) in that order.
        /// @param descs The descriptions of the acceleration structures which will be assigned a region of the scratch
        /// buffer
        /// @return The new scratch buffer
        ComPtr<DMA::Allocation> AllocateAndAssignScratchBuffer(std::vector<AccelerationStructureDesc>& descs);

        /// @brief Allocate a scratch buffer of the given size
        /// @param size The size of the scratch buffer
        /// @return The new scratch buffer
        ComPtr<DMA::Allocation> AllocateScratchBuffer(UINT64 size);

        /// @brief Allocate a instance buffer for a top level acceleration structure. These must then be filled with
        /// valid instance descriptions when building the acceleration structure. Can be discarded after building, but
        /// better if reused every frame.
        /// @param numInstances The number of instances to allocate space for
        /// @return The new instance buffer
        ComPtr<DMA::Allocation> AllocateInstanceBuffer(UINT64 numInstances,
                                                       D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_UPLOAD);

        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@ Ray Tracing Pipeline @@@@@@@@@@@@@@@@@@@@@@@@@
        // @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

        /// Information about Ray Tracing Pipelines
        /// DXRay doesn't provide any abstraction for ray tracing pipelines other than creation / expanding
        /// This is because there is already enough abstraction in the D3D12 API by CD3DX12_STATE_OBJECT_DESC and it is
        /// not worth the effort to create a new abstraction layer for it as that is abstract enough for most use cases
        /// and the library doesn't want to limit the user in any way.

        /// @brief Create a ray tracing pipeline. This can take precious cpu time, so it is recommended compile
        /// pipelines in parallel in other threads, but if it is small enough performance should be fine. Ideally
        /// pipelines should be compiled once and added to incrementally, mitigating the performance hit to recompile
        /// the whole pipeline again. This is done by using the D3D12_STATE_OBJECT_FLAG_ALLOW_STATE_OBJECT_ADDITION flag
        /// when creating the pipeline and then using ExpandPipeline(...) to add a collection type state object to the
        /// pipeline.
        /// @param desc The description of the pipeline
        /// @return The new pipeline
        ComPtr<ID3D12StateObject> CreatePipeline(CD3DX12_STATE_OBJECT_DESC& desc);

        /// @brief Expand a pipeline by adding a collection type state object to it.
        /// Both pipelines should be created with the D3D12_STATE_OBJECT_FLAG_ALLOW_STATE_OBJECT_ADDITION flag.
        /// @param desc The description of the pipeline, should be of type D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE
        /// and should contain D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION in the list of subobjects.
        /// @param pipeline The pipeline to expand, should be of type D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE
        /// @param collection The collection to add to the pipeline, should be of type
        /// D3D12_STATE_OBJECT_TYPE_COLLECTION
        /// @return The new expanded pipeline
        ComPtr<ID3D12StateObject> ExpandPipeline(CD3DX12_STATE_OBJECT_DESC& desc, ComPtr<ID3D12StateObject>& pipeline,
                                                 ComPtr<ID3D12StateObject>& collection);

        /// @brief Create a shader table
        /// @param table The shader table to create, filled with the shader names, etc.
        /// @param heap The heap type to use for the shader table. This heap should be CPU accessible.
        /// GPU_UPLOAD or UPLOAD are recommended, and if you really want to use DEFAULT, the table can act as a UPLOAD
        /// buffer and copy the whole buffer via CopyBufferRegion(...) to a DEFAULT buffer.
        /// @param pipeline The pipeline to use for the shader table
        void CreateShaderTable(ShaderTable& table, D3D12_HEAP_TYPE heap, ComPtr<ID3D12StateObject>& pipeline);

        /// @todo Add support for shader table expansion
        /// @todo Add support for copying shader tables to DEFAULT heap
        /// @todo Add support for writing to local root signatures


private: // Internal methods
        /// @brief Allocate a bottom level acceleration structure, used by AllocateAccelerationStructure(...) if
        /// the type is bottom level
        ComPtr<DMA::Allocation> InternalAllocateBottomAccelerationStructure(AccelerationStructureDesc& desc);

        /// @brief Allocate a top level acceleration structure, used by AllocateAccelerationStructure(...) if the
        /// type is top level
        ComPtr<DMA::Allocation> InternalAllocateTopAccelerationStructure(AccelerationStructureDesc& desc);

    private:
        /// @brief The D3D12 device
        ComPtr<DXRDevice> mDevice = nullptr;

        /// @brief The DXGI adapter
        ComPtr<DXRAdapter> mAdapter = nullptr;

        /// @brief The allocator to use for all allocations
        ComPtr<DMA::Allocator> mAllocator = nullptr;

        /// @brief The pool to use for all allocations
        DMA::Pool* mPool = nullptr;
    };
} // namespace DXR
