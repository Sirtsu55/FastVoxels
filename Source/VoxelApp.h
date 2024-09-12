#include "Common.h"
#include "ShaderCompiler.h"
#include "Application.h"

struct PerformanceData
{
    uint32_t Frame;
    DOUBLE FrameTime;
};

struct VoxAABB
{
    glm::vec3 Min;
    glm::vec3 Max;
    uint32_t ColorIndex;
    uint32_t Padding;
};

struct VoxMaterial
{
    uint32_t Color;
    float Emissive;
};

struct VoxelModel
{
    glm::vec3 Size;
    glm::mat3x4 Transform;
    std::vector<VoxAABB> AABBs;
};

struct VoxelScene
{
    std::vector<ComPtr<DMA::Allocation>> ModelBuffers;
    ComPtr<DMA::Allocation> InstanceBuffer;

    ComPtr<DMA::Allocation> SizeBuffer;
    ComPtr<DMA::Allocation> ColorBuffer;

    std::vector<DXR::AccelerationStructureDesc> BLASDescs;
    DXR::AccelerationStructureDesc TLASDesc;

    std::vector<ComPtr<DMA::Allocation>> BLAS;
    ComPtr<DMA::Allocation> TLAS;

    ComPtr<DMA::Allocation> ScratchBufferBLAS;
    ComPtr<DMA::Allocation> ScratchBufferTLAS;

    std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> AABBViews;

    std::uint64_t NumVoxels = 0;
    std::uint64_t ASMemoryConsumption = 0;
    std::uint64_t BuffersMemoryConsumption = 0;
};

struct SceneConfig
{
    glm::vec3 CameraPosition = {0.0f, 0.0f, 0.0f};
    glm::vec3 CameraDirection = {0.0f, 0.0f, 1.0f};
    float SceneLightIntensity = 1.0f;
    float SkyBrightness = 1.0f;
};

class AxisAlignedIntersection : public Application
{
public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void Stop() override;

    virtual void EndFrame() override;

private:
    void WritePerformanceData();

public:
    ShaderCompiler mShaderCompiler;

    std::shared_ptr<VoxelScene> mScene;

    DXR::ShaderTable mShaderTable;
    ComPtr<ID3D12StateObject> mPipeline;
    ComPtr<ID3D12RootSignature> mRootSig;

    DOUBLE mTimestampFrequency = 0.0;

    uint32_t mBenchmarkFrameCount = UINT16_MAX;

    std::vector<PerformanceData> mPerformanceData;
    std::ofstream mPerformanceFile;
};
