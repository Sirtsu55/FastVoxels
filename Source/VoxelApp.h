#include "Common.h"
#include "ShaderCompiler.h"
#include "Application.h"

struct PerformanceData
{
	DOUBLE ApplicationTime;
	DOUBLE PerformanceTime;
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

	ComPtr<DMA::Allocation> mBLAS;
	ComPtr<DMA::Allocation> mTLAS;

	DXR::ShaderTable mShaderTable;
	ComPtr<ID3D12StateObject> mPipeline;
	ComPtr<ID3D12RootSignature> mRootSig;

	ComPtr<DMA::Allocation> mAABBBuffer;
	ComPtr<DMA::Allocation> mInstanceBuffer;

	ComPtr<ID3D12QueryHeap> mQueryHeap;
	ComPtr<DMA::Allocation> mQueryOutput;
	UINT64* mQueryOutputData = nullptr;

	DOUBLE mTimestampFrequency = 0.0;

	std::vector<PerformanceData> mPerformanceData;
	std::ofstream mPerformanceFile;
};