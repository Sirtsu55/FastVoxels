#include "Base/Common.h"
#include "Base/ShaderCompiler.h"
#include "Base/Application.h"
#include "Base/FileRead.h"

#define OGT_VOX_IMPLEMENTATION
#include "Base/ogt_vox.h"

struct PerformanceData
{
	DOUBLE ApplicationTime;
	DOUBLE PerformanceTime;
};

class BoxIntersections : public Application
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


void BoxIntersections::Start()
{
	// Write the header to the file
	mPerformanceData.reserve(100);
	mPerformanceFile.open("PerformanceData.csv", std::ios::out);
	mPerformanceFile << "Frame (n),Performance Time (ms)" << std::endl;

	// Create the pipeline
	auto dxil = mShaderCompiler.CompileFromFile("Shaders/BasicShader.hlsl");
	assert(dxil != nullptr);
	CD3DX12_SHADER_BYTECODE shader{ dxil->GetBufferPointer(), dxil->GetBufferSize() };

	// Create the root signature
	mDXDevice->CreateRootSignature(0, shader.pShaderBytecode, shader.BytecodeLength, IID_PPV_ARGS(&mRootSig));

	CD3DX12_STATE_OBJECT_DESC rtPipeline(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

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
	lib->DefineExport(L"RootSig");

	// Export the root signature
	mPipeline = mDevice->CreatePipeline(rtPipeline);

	mShaderTable.AddShader(L"rgen", DXR::ShaderType::RayGen);
	mShaderTable.AddShader(L"miss", DXR::ShaderType::Miss);
	mShaderTable.AddShader(L"AABBHitGroup", DXR::ShaderType::HitGroup);

	mDevice->CreateShaderTable(mShaderTable, D3D12_HEAP_TYPE_GPU_UPLOAD, mPipeline);

	// Create AS
	std::vector<uint8_t> rawVox;
	FileRead("Assets/cars.vox", rawVox);
	auto voxScene = ogt_vox_read_scene(rawVox.data(), rawVox.size());

	std::vector<glm::vec3> positions = {};

	for (uint32_t i = 0; i < voxScene->num_instances; i++)
	{
		auto& instance = voxScene->instances[i];
		auto& model = voxScene->models[instance.model_index];

		glm::mat4 transform = glm::make_mat4(&instance.transform.m00);
		transform = glm::transpose(transform);

		uint32_t sizeX = model->size_x;
		uint32_t sizeY = model->size_y;
		uint32_t sizeZ = model->size_z;

		// Iterate over the voxels in the model
		for (uint32_t x = 0; x < sizeX; x++)
		{
			for (uint32_t y = 0; y < sizeY; y++)
			{
				for (uint32_t z = 0; z < sizeZ; z++)
				{
					if (model->voxel_data[x + y * sizeX + z * sizeX * sizeY] != 0)
					{
						auto pos = transform * glm::vec4(x, y, z, 1.0f);

						positions.push_back(glm::vec3(pos));
					}
				}
			}
		}
	}

	auto allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_RAYTRACING_AABB) * positions.size(), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
	mAABBBuffer = mDevice->AllocateResource(allocDesc, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_HEAP_TYPE_GPU_UPLOAD);

	// Fill the AABB buffer
	D3D12_RAYTRACING_AABB* aabbs = (D3D12_RAYTRACING_AABB*)mDevice->MapAllocationForWrite(mAABBBuffer);


	for (UINT32 i = 0; i < positions.size(); i++)
	{
		auto pos = positions[i];

		const float voxSide = 0.5;

		aabbs[i].MinX = pos.x - voxSide;
		aabbs[i].MinY = pos.y - voxSide;
		aabbs[i].MinZ = pos.z - voxSide;
		aabbs[i].MaxX = pos.x + voxSide;
		aabbs[i].MaxY = pos.y + voxSide;
		aabbs[i].MaxZ = pos.z + voxSide;
	}


	mAABBBuffer->GetResource()->Unmap(0, nullptr);

	DXR::AccelerationStructureDesc blasDesc = {};
	blasDesc.Geometries.push_back(
		D3D12_RAYTRACING_GEOMETRY_DESC{
			.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS,
			.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
			.AABBs = D3D12_RAYTRACING_GEOMETRY_AABBS_DESC{
				.AABBCount = positions.size(),
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
	transform = glm::translate(transform, glm::vec3(-10.0f, -10.0f, -50.0f));
	transform = glm::transpose(transform);

	allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	mInstanceBuffer = mDevice->AllocateInstanceBuffer(1, D3D12_HEAP_TYPE_GPU_UPLOAD);

	D3D12_RAYTRACING_INSTANCE_DESC* instances = (D3D12_RAYTRACING_INSTANCE_DESC*)mDevice->MapAllocationForWrite(mInstanceBuffer);

	instances[0].InstanceID = 0;
	instances[0].InstanceContributionToHitGroupIndex = 0;
	instances[0].InstanceMask = 0xFF;
	instances[0].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
	instances[0].AccelerationStructure = mBLAS->GetResource()->GetGPUVirtualAddress();
	memcpy(instances[0].Transform, glm::value_ptr(transform), sizeof(FLOAT) * 12);

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

	// Create the query heap
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.NodeMask = 0;
	queryHeapDesc.Count = 2;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	THROW_IF_FAILED(mDXDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&mQueryHeap)));

	// Create the output buffer
	allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
	mQueryOutput = mDevice->AllocateResource(allocDesc, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
	mQueryOutputData = (UINT64*)mDevice->MapAllocationForWrite(mQueryOutput);

	// Get the timestamp frequency
	UINT64 frequency;
	THROW_IF_FAILED(mCommandQueue->GetTimestampFrequency(&frequency));
	mTimestampFrequency = frequency;
	
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mResourceHeap->GetCPUDescriptorHandleForHeapStart());
	cpuHandle.Offset(3, mResourceDescriptorSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = positions.size();
	srvDesc.Buffer.StructureByteStride = sizeof(D3D12_RAYTRACING_AABB);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	mDXDevice->CreateShaderResourceView(mAABBBuffer->GetResource(), &srvDesc, cpuHandle);
}

void BoxIntersections::Update()
{
	mCommandList->SetPipelineState1(mPipeline.Get());

	mCommandList->SetComputeRootSignature(mRootSig.Get());

	mCommandList->SetComputeRootShaderResourceView(0, mTLAS->GetResource()->GetGPUVirtualAddress());

	mCommandList->EndQuery(mQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

	D3D12_DISPATCH_RAYS_DESC desc = mShaderTable.GetRaysDesc(0, mWidth, mHeight);
	mCommandList->DispatchRays(&desc);

	mCommandList->EndQuery(mQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

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

	mCommandList->ResolveQueryData(mQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, mQueryOutput->GetResource(), 0);

	// Calculate the time taken
	PerformanceData data;
	data.ApplicationTime = glfwGetTime();
	data.PerformanceTime = ((mQueryOutputData[1] - mQueryOutputData[0]) / mTimestampFrequency) * 1000.0; // ms
	mPerformanceData.push_back(data);

	if (mPerformanceData.size() >= 100)
	{
		WritePerformanceData();
		mPerformanceData.clear();
	}
}

void BoxIntersections::WritePerformanceData()
{
	for (auto& data : mPerformanceData)
	{
		mPerformanceFile << data.ApplicationTime << "," << data.PerformanceTime << std::endl;
	}
}

void BoxIntersections::Stop()
{
	// Write the performance data to the file
	WritePerformanceData();
	mPerformanceFile.close();
}

int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
    Application* app = new BoxIntersections();

    app->Run();

    delete app;
}