#include "Common.h"
#include "ShaderCompiler.h"
#include "Application.h"

struct PerformanceData
{
    DOUBLE ApplicationTime;
    DOUBLE PerformanceTime;
};

struct VoxelModel
{
    glm::mat3x4 Transform;
    std::vector<D3D12_RAYTRACING_AABB> AABBs;
};

struct VoxelScene
{
    std::vector<ComPtr<DMA::Allocation>> ModelBuffers;
    ComPtr<DMA::Allocation> InstanceBuffer;

    std::vector<DXR::AccelerationStructureDesc> BLASDescs;
    DXR::AccelerationStructureDesc TLASDesc;

    std::vector<ComPtr<DMA::Allocation>> BLAS;
    ComPtr<DMA::Allocation> TLAS;

    ComPtr<DMA::Allocation> ScratchBuffer;

    std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> SRVs;
};

class VoxelApp : public Application
{
public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void Stop() override;

private:
    void WritePerformanceData();

public:
    ShaderCompiler mShaderCompiler;

    std::shared_ptr<VoxelScene> mScene;

    DXR::ShaderTable mShaderTable;
    ComPtr<ID3D12StateObject> mPipeline;
    ComPtr<ID3D12RootSignature> mRootSig;

    ComPtr<ID3D12QueryHeap> mQueryHeap;
    ComPtr<DMA::Allocation> mQueryOutput;
    UINT64* mQueryOutputData = nullptr;

    DOUBLE mTimestampFrequency = 0.0;

    std::vector<PerformanceData> mPerformanceData;
    std::ofstream mPerformanceFile;
};
