#include "Common.h"
#include "DistanceFieldIntersection/DistanceFieldIntersection.h"
#include "FileRead.h"
#include "ogt_vox.h"

std::shared_ptr<VoxelScene> LoadAsAABBs(std::shared_ptr<DXR::Device> device, const std::string& voxFile)
{
    auto scene = std::make_shared<VoxelScene>();

    std::vector<uint8_t> rawVox;
    FileRead("Assets/gym.vox", rawVox);
    auto voxScene = ogt_vox_read_scene(rawVox.data(), rawVox.size());
    rawVox.clear();

    // Color Buffer for the voxels
    auto colorBufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(256 * sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    scene->ColorBuffer =
        device->AllocateResource(colorBufferDesc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_GPU_UPLOAD);

    // copy the colors to the buffer
    uint8_t* colors = (uint8_t*)device->MapAllocationForWrite(scene->ColorBuffer);
    memcpy(colors, voxScene->palette.color, 256 * sizeof(uint32_t));

    auto sizeBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(voxScene->num_instances * sizeof(glm::vec4),
                                                        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    scene->SizeBuffer =
        device->AllocateResource(sizeBufferDesc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_GPU_UPLOAD);

    std::vector<VoxelModel> models;
    // Keep track of the total size needed for the buffer
    for (uint32_t i = 0; i < voxScene->num_instances; i++)
    {
        auto& instance = voxScene->instances[i];
        auto& model = voxScene->models[instance.model_index];
        auto& group = voxScene->groups[instance.group_index];

        VoxelModel& voxelModel = models.emplace_back();

        glm::mat4 instanceTransform = glm::make_mat4(&instance.transform.m00);
        glm::mat4 groupTransform = glm::make_mat4(&group.transform.m00);

        glm::mat4 modelTransform = instanceTransform * groupTransform;

        const uint32_t sizeX = model->size_x;
        const uint32_t sizeY = model->size_y;
        const uint32_t sizeZ = model->size_z;

        voxelModel.Size = glm::vec3(sizeX, sizeY, sizeZ);

        glm::vec3 trans = -glm::vec3(sizeX / 2.0, sizeY / 2.0, sizeZ / 2.0);

        modelTransform = glm::translate(modelTransform, trans);
        voxelModel.Transform = glm::transpose(modelTransform);

        // Iterate over the voxels in the model
        for (uint32_t x = 0; x < sizeX; x++)
        {
            for (uint32_t y = 0; y < sizeY; y++)
            {
                for (uint32_t z = 0; z < sizeZ; z++)
                {
                    uint8_t color = model->voxel_data[x + y * sizeX + z * sizeX * sizeY];
                    if (color != 0)
                    {
                        const float voxSize = 0.5;

                        glm::vec3 mid = glm::vec3(x, y, z);

                        auto& aabb = voxelModel.AABBs.emplace_back();
                        aabb.ColorIndex = color;
                        aabb.Max = mid + glm::vec3(voxSize);
                        aabb.Min = mid - glm::vec3(voxSize);
                    }
                }
            }
        }
    }

    glm::vec3* sizes = (glm::vec3*)device->MapAllocationForWrite(scene->SizeBuffer);
    for (auto& model : models)
    {
        // All the AABBs for the model
        auto allocDesc = CD3DX12_RESOURCE_DESC::Buffer(model.AABBs.size() * sizeof(VoxAABB),
                                                       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto& modelBuffer = scene->ModelBuffers.emplace_back(
            device->AllocateResource(allocDesc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_GPU_UPLOAD));

        uint8_t* aabbs = (uint8_t*)device->MapAllocationForWrite(modelBuffer);
        uint64_t gpuAddress = modelBuffer->GetResource()->GetGPUVirtualAddress();

        auto& blas = scene->BLASDescs.emplace_back();
        blas.Geometries.push_back(D3D12_RAYTRACING_GEOMETRY_DESC {
            .Type = D3D12_RAYTRACING_GEOMETRY_TYPE_PROCEDURAL_PRIMITIVE_AABBS,
            .Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE,
            .AABBs =
                D3D12_RAYTRACING_GEOMETRY_AABBS_DESC {
                    .AABBCount = model.AABBs.size(),
                    .AABBs = D3D12_GPU_VIRTUAL_ADDRESS_AND_STRIDE {.StartAddress = gpuAddress,
                                                                   .StrideInBytes = sizeof(VoxAABB)}},
        });

        memcpy(aabbs, model.AABBs.data(), model.AABBs.size() * sizeof(VoxAABB));
        memcpy(sizes, glm::value_ptr(model.Size), sizeof(glm::vec3));
        sizes++; // Move to the next size

        // Create the BLAS
        scene->BLAS.push_back(device->AllocateAccelerationStructure(blas));

        // SRV for the AABB buffer
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = model.AABBs.size();
        srvDesc.Buffer.StructureByteStride = sizeof(VoxAABB);
        srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        scene->AABBViews.push_back(srvDesc);
    }

    // Create TLAS for the models
    scene->InstanceBuffer = device->AllocateInstanceBuffer(models.size(), D3D12_HEAP_TYPE_GPU_UPLOAD);
    D3D12_RAYTRACING_INSTANCE_DESC* instances =
        (D3D12_RAYTRACING_INSTANCE_DESC*)device->MapAllocationForWrite(scene->InstanceBuffer);

    // Create the instances
    for (uint32_t i = 0; i < models.size(); i++)
    {
        auto& model = models[i];

        instances[i].InstanceID = i; // this is the buffer index
        instances[i].InstanceContributionToHitGroupIndex = 0;
        instances[i].InstanceMask = 0xFF;
        instances[i].Flags = D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
        instances[i].AccelerationStructure = scene->BLAS[i]->GetResource()->GetGPUVirtualAddress();
        memcpy(instances[i].Transform, glm::value_ptr(model.Transform), sizeof(FLOAT) * 12);
    }

    scene->InstanceBuffer->GetResource()->Unmap(0, nullptr);

    // Create the TLAS
    scene->TLASDesc.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    scene->TLASDesc.vpInstanceDescs = scene->InstanceBuffer->GetResource()->GetGPUVirtualAddress();
    scene->TLASDesc.NumInstanceDescs = models.size();

    scene->TLAS = device->AllocateAccelerationStructure(scene->TLASDesc);

    // Scratch buffer for BLAS
    scene->ScratchBufferBLAS = device->AllocateAndAssignScratchBuffer(scene->BLASDescs);

    // Scratch buffer for TLAS
    scene->ScratchBufferTLAS = device->AllocateAndAssignScratchBuffer(scene->TLASDesc);

    ogt_vox_destroy_scene(voxScene);

    return scene;
}

void AxisAlignedIntersection::Start()
{
    // Write the header to the file
    mPerformanceData.reserve(100);
    mPerformanceFile.open("PerformanceData.csv", std::ios::out);
    mPerformanceFile << "Frame (n),Performance Time (ms)" << std::endl;

    // Create the pipeline
    auto dxil = mShaderCompiler.CompileFromFile("Shaders/AxisAlignedIntersection.hlsl");
    assert(dxil != nullptr);
    CD3DX12_SHADER_BYTECODE shader {dxil->GetBufferPointer(), dxil->GetBufferSize()};

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
    mScene = LoadAsAABBs(mDevice, "Assets/CountrySide_Source.vox");

    // Build the acceleration structures
    THROW_IF_FAILED(mCommandAllocators[mBackBufferIndex]->Reset());
    THROW_IF_FAILED(mCommandList->Reset(mCommandAllocators[mBackBufferIndex].Get(), nullptr));

    std::vector<D3D12_RESOURCE_BARRIER> barriers(mScene->BLAS.size());
    for (uint32_t i = 0; i < mScene->BLAS.size(); i++)
    {
        auto& blasDesc = mScene->BLASDescs[i];
        auto& blas = mScene->BLAS[i];
        mDevice->BuildAccelerationStructure(blasDesc, mCommandList);
        barriers[i] = CD3DX12_RESOURCE_BARRIER::UAV(blas->GetResource());
    }

    // Barrier
    mCommandList->ResourceBarrier(barriers.size(), barriers.data());

    mDevice->BuildAccelerationStructure(mScene->TLASDesc, mCommandList);

    mCommandList->Close();

    ID3D12CommandList* ppCommandLists[] = {mCommandList.Get()};
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
    auto allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);
    mQueryOutput = mDevice->AllocateResource(allocDesc, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_READBACK);
    mQueryOutputData = (UINT64*)mDevice->MapAllocationForWrite(mQueryOutput);

    // Get the timestamp frequency
    UINT64 frequency;
    THROW_IF_FAILED(mCommandQueue->GetTimestampFrequency(&frequency));
    mTimestampFrequency = frequency;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mResourceHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(3, mResourceDescriptorSize);

    // Color buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = 256;
    srvDesc.Buffer.StructureByteStride = 0;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;

    mDXDevice->CreateShaderResourceView(mScene->ColorBuffer->GetResource(), &srvDesc, cpuHandle);

    cpuHandle.Offset(1, mResourceDescriptorSize);

    for (uint32_t i = 0; i < mScene->ModelBuffers.size(); i++)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = mScene->AABBViews[i];
        auto& modelBuffer = mScene->ModelBuffers[i];

        mDXDevice->CreateShaderResourceView(modelBuffer->GetResource(), &srvDesc, cpuHandle);
        cpuHandle.Offset(1, mResourceDescriptorSize);
    }
}

void AxisAlignedIntersection::Update()
{
    mCommandList->SetPipelineState1(mPipeline.Get());

    mCommandList->SetComputeRootSignature(mRootSig.Get());

    mCommandList->SetComputeRootShaderResourceView(0, mScene->TLAS->GetResource()->GetGPUVirtualAddress());

    mCommandList->EndQuery(mQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

    D3D12_DISPATCH_RAYS_DESC desc = mShaderTable.GetRaysDesc(0, mWidth, mHeight);
    mCommandList->DispatchRays(&desc);

    mCommandList->EndQuery(mQueryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    // Copy the output image to the back buffer
    D3D12_RESOURCE_BARRIER barriers[] = {
        CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffers[mBackBufferIndex].Get(), D3D12_RESOURCE_STATE_PRESENT,
                                             D3D12_RESOURCE_STATE_COPY_DEST),
        CD3DX12_RESOURCE_BARRIER::Transition(mOutputImage->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                             D3D12_RESOURCE_STATE_COPY_SOURCE),
    };
    mCommandList->ResourceBarrier(2, barriers);

    mCommandList->CopyResource(mBackBuffers[mBackBufferIndex].Get(), mOutputImage->GetResource());

    barriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(mBackBuffers[mBackBufferIndex].Get(),
                                                       D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);
    barriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(mOutputImage->GetResource(), D3D12_RESOURCE_STATE_COPY_SOURCE,
                                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
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
    }
}

void AxisAlignedIntersection::WritePerformanceData()
{
    for (auto& data : mPerformanceData)
    {
        mPerformanceFile << data.ApplicationTime << "," << data.PerformanceTime << std::endl;
    }
    mPerformanceData.clear();
}

void AxisAlignedIntersection::Stop()
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
    Application* app = new AxisAlignedIntersection();

    app->Run();

    delete app;
}
