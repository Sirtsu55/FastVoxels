#include "Base/Common.h"
#include "Base/ShaderCompiler.h"
#include "Base/Application.h"
#include "Base/FileRead.h"

#define OGT_VOX_IMPLEMENTATION
#include "Base/ogt_vox.h"

struct PerformanceData
{
    double ApplicationTime;
	double PerformanceTime;
};

class BoxIntersections : public Application
{
public:
    virtual void Start() override;
    virtual void Update() override;
    virtual void Stop() override;


public:
    ShaderCompiler mShaderCompiler;

	ComPtr<DMA::Allocation> mBLAS;
	ComPtr<DMA::Allocation> mTLAS;

	DXR::ShaderTable mShaderTable;
	ComPtr<ID3D12StateObject> mPipeline;
	ComPtr<ID3D12RootSignature> mRootSig;

	ComPtr<DMA::Allocation> mAABBBuffer;
	ComPtr<DMA::Allocation> mInstanceBuffer;


    std::ofstream mPerformanceFile;

    std::vector<PerformanceData> mPerformanceData;
};


void BoxIntersections::Start()
{
    // Write the header to the file
    mPerformanceData.reserve(100);
    mPerformanceFile.open("PerformanceData.csv", std::ios::out);
    mPerformanceFile << "Application Time (s),Performance Time (ms)" << std::endl;

    // Create bindless root sig 
	ComPtr<ID3DBlob> rootSigBlob;
	ComPtr<ID3DBlob> errorBlob;
	D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
	rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED;
	D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &errorBlob);

	if (errorBlob != nullptr)
	{
		std::string error = (char*)errorBlob->GetBufferPointer();
		std::cout << error << std::endl;
	}

	mDXDevice->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&mRootSig));

	// Create the pipeline
    auto dxil = mShaderCompiler.CompileFromFile("Shaders/BasicShader.hlsl");
	assert(dxil != nullptr);
	CD3DX12_SHADER_BYTECODE shader{ dxil->GetBufferPointer(), dxil->GetBufferSize() };

	CD3DX12_STATE_OBJECT_DESC rtPipeline(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

	// Add the root signature
	auto* rootSig = rtPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	rootSig->SetRootSignature(mRootSig.Get());

	// Add the DXIL library
    auto* lib = rtPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
	lib->SetDXILLibrary(&shader);
	lib->DefineExport(L"rgen");
    lib->DefineExport(L"miss");
    lib->DefineExport(L"chit");
    lib->DefineExport(L"isect");
	lib->DefineExport(L"AABBHitGroup");
	lib->DefineExport(L"ShaderConfig");
	lib->DefineExport(L"PipelineConfig");

	// Export the root signature
	mPipeline = mDevice->CreatePipeline(rtPipeline);

	mShaderTable.AddShader(L"rgen", DXR::ShaderType::RayGen);
	mShaderTable.AddShader(L"miss", DXR::ShaderType::Miss);
	mShaderTable.AddShader(L"AABBHitGroup", DXR::ShaderType::HitGroup);

	mDevice->CreateShaderTable(mShaderTable, D3D12_HEAP_TYPE_GPU_UPLOAD, mPipeline);

	// Create AS
	uint32_t numBoxes = 1;

	auto allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_RAYTRACING_AABB) * numBoxes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mAABBBuffer = mDevice->AllocateResource(allocDesc, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_GPU_UPLOAD);

	// Fill the AABB buffer
	D3D12_RAYTRACING_AABB* aabbs = (D3D12_RAYTRACING_AABB*)mDevice->MapAllocationForWrite(mAABBBuffer);
	aabbs[0].MinX = -1.0f;
	aabbs[0].MinY = -1.0f;
	aabbs[0].MinZ = -1.0f;
	aabbs[0].MaxX = 1.0f;
	aabbs[0].MaxY = 1.0f;
	aabbs[0].MaxZ = 1.0f;

	mAABBBuffer->GetResource()->Unmap(0, nullptr);

	DXR::AccelerationStructureDesc blasDesc = {};
	blasDesc.Geometries.push_back(
		D3D12_RAYTRACING_GEOMETRY_DESC{
			.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS,
			.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
			.AABBs = D3D12_RAYTRACING_GEOMETRY_AABBS_DESC{
				.AABBCount = 1,
				.AABBs = D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE{
					.StartAddress = mAABBBuffer->GetResource()->GetGPUVirtualAddress(),
					.StrideInBytes = sizeof(D3D12_RAYTRACING_AABB)
				}
			},
		}
	);

	mBLAS = mDevice->AllocateAccelerationStructure(blasDesc);

	// Create TLAS

	glm::mat4 transform = glm::mat4(1.0f);

	allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mInstanceBuffer = mDevice->AllocateInstanceBuffer(1, D3D12_HEAP_TYPE_GPU_UPLOAD);

	D3D12_RAYTRACING_INSTANCE_DESC* instances = (D3D12_RAYTRACING_INSTANCE_DESC*)mDevice->MapAllocationForWrite(mInstanceBuffer);

	instances[0].InstanceID = 0;
	instances[0].InstanceContributionToHitGroupIndex = 0;
	instances[0].InstanceMask = 0xFF;
	instances[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
	instances[0].AccelerationStructure = mBLAS->GetResource()->GetGPUVirtualAddress();
	memcpy(instances[0].Transform, glm::value_ptr(transform), sizeof(float) * 12);

	mInstanceBuffer->GetResource()->Unmap(0, nullptr);

	DXR::AccelerationStructureDesc tlasDesc = {};
	tlasDesc.NumInstanceDescs = 1;
	tlasDesc.vpInstanceDescs = mInstanceBuffer->GetResource()->GetGPUVirtualAddress();

	mTLAS = mDevice->AllocateAccelerationStructure(tlasDesc);

	ComPtr<DMA::Allocation> scratchBuffer = mDevice->AllocateScratchBuffer(blasDesc.GetScratchBufferSize() + tlasDesc.GetScratchBufferSize());
	blasDesc.GetBuildDesc().ScratchAccelerationStructureData = scratchBuffer->GetResource()->GetGPUVirtualAddress();
	tlasDesc.GetBuildDesc().ScratchAccelerationStructureData = scratchBuffer->GetResource()->GetGPUVirtualAddress();

	// Build the acceleration structures
	THROW_IF_FAILED(mCommandAllocators[mBackBufferIndex]->Reset());
	THROW_IF_FAILED(mCommandList->Reset(mCommandAllocators[mBackBufferIndex].Get(), nullptr));

	mDevice->BuildAccelerationStructure(blasDesc, mCommandList);

	// Barrier
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(mBLAS->GetResource());
	mCommandList->ResourceBarrier(1, &barrier);

	mDevice->BuildAccelerationStructure(tlasDesc, mCommandList);

	mCommandList->Close();

	ID3D12CommandList* ppCommandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(1, ppCommandLists);

	mCommandQueue->Signal(mFence.Get(), ++mFenceValue);

	// Wait for the command list to finish
	mFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
	WaitForSingleObject(mFenceEvent, INFINITE);

	// Create descriptors
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mResourceHeap->GetCPUDescriptorHandleForHeapStart(), 3, mResourceDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.RaytracingAccelerationStructure.Location = mTLAS->GetResource()->GetGPUVirtualAddress();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	mDXDevice->CreateShaderResourceView(nullptr, &srvDesc, cpuHandle);
}

void BoxIntersections::Update()
{
	mCommandList->SetPipelineState1(mPipeline.Get());

	D3D12_DISPATCH_RAYS_DESC desc = mShaderTable.GetRaysDesc(0, mWidth, mHeight);
	mCommandList->DispatchRays(&desc);

	// Cop the output image to the back buffer
	D3D12_RESOURCE_BARRIER barriers[] = {
		CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffers[mBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST),
		CD3DX12_RESOURCE_BARRIER::Transition(mOutputImage->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE),
	};
	mCommandList->ResourceBarrier(2, barriers);

	mCommandList->CopyResource(mBackBuffers[mBackBufferIndex].Get(), mOutputImage->GetResource());

	barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffers[mBackBufferIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
	barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputImage->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	mCommandList->ResourceBarrier(2, barriers);
}

void BoxIntersections::Stop()
{

}

int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
    Application *app = new BoxIntersections();

    app->Run();

    delete app;
}