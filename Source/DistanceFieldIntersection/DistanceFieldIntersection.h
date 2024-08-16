#include "Common.h"
#include "ShaderCompiler.h"
#include "Application.h"
#include <cstdint>

struct PerformanceData
{
    DOUBLE ApplicationTime;
    DOUBLE PerformanceTime;
};

struct Voxel
{
    uint32_t Distance : 4;
    uint32_t ColorIndex : 28;
};

struct VoxMaterial
{
    uint32_t Color;
    float Emissive;
};

struct VoxelModel
{
    std::vector<Voxel> Voxels;
    glm::vec3 Size;
    glm::mat3x4 Transform;
};

struct VoxelScene
{
    std::vector<ComPtr<DMA::Allocation>> DistanceFieldTextures;

    ComPtr<DMA::Allocation> ColorBuffer;
    ComPtr<DMA::Allocation> AABBBuffer;

    std::vector<DXR::AccelerationStructureDesc> BLASDescs;
    DXR::AccelerationStructureDesc TLASDesc;

    std::vector<ComPtr<DMA::Allocation>> BLAS;
    ComPtr<DMA::Allocation> TLAS;
    ComPtr<DMA::Allocation> InstanceBuffer;

    ComPtr<DMA::Allocation> ScratchBufferBLAS;
    ComPtr<DMA::Allocation> ScratchBufferTLAS;

    uint64_t NumVoxels = 0;
};

class DistanceFieldIntersection : public Application
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
