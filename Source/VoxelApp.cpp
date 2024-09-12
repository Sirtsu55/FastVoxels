#include "VoxelApp.h"
#include "Common.h"
#include "FileRead.h"
#include "ogt_vox.h"
#include "toml++/toml.hpp"

std::shared_ptr<VoxelScene> LoadAsAABBs(std::shared_ptr<DXR::Device> device, const std::string& voxFile)
{
    auto scene = std::make_shared<VoxelScene>();

    std::vector<uint8_t> rawVox;
    FileRead(voxFile, rawVox);
    auto voxScene = ogt_vox_read_scene(rawVox.data(), rawVox.size());
    rawVox.clear();

    // Color Buffer for the voxels
    auto colorBufferDesc =
        CD3DX12_RESOURCE_DESC::Buffer(256 * sizeof(VoxMaterial), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    scene->ColorBuffer =
        device->AllocateResource(colorBufferDesc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_GPU_UPLOAD);

    // copy the colors to the buffer
    VoxMaterial* colors = (VoxMaterial*)device->MapAllocationForWrite(scene->ColorBuffer);
    for (uint32_t i = 0; i < 256; i++)
    {
        // Type punning
        colors[i].Color = *(uint32_t*)&voxScene->palette.color[i];
        colors[i].Emissive = voxScene->materials.matl[i].emit;
    }

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

                        scene->NumVoxels++;
                    }
                }
            }
        }
    }

    for (auto& model : models)
    {
        // All the AABBs for the model
        auto allocDesc = CD3DX12_RESOURCE_DESC::Buffer(model.AABBs.size() * sizeof(VoxAABB),
                                                       D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        auto& modelBuffer = scene->ModelBuffers.emplace_back(
            device->AllocateResource(allocDesc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_GPU_UPLOAD));

        scene->BuffersMemoryConsumption += modelBuffer->GetSize();

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

        // Create the BLAS
        auto& allocBLAS = scene->BLAS.emplace_back(device->AllocateAccelerationStructure(blas));
        scene->ASMemoryConsumption += allocBLAS->GetSize();

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
    scene->ASMemoryConsumption += scene->TLAS->GetSize();

    // Scratch buffer for BLAS
    scene->ScratchBufferBLAS = device->AllocateAndAssignScratchBuffer(scene->BLASDescs);

    // Scratch buffer for TLAS
    scene->ScratchBufferTLAS = device->AllocateAndAssignScratchBuffer(scene->TLASDesc);

    ogt_vox_destroy_scene(voxScene);

    return scene;
}

void AxisAlignedIntersection::Start()
{
    auto config = toml::parse_file("Data/config.toml");

    // Create the pipeline
    std::string_view file = config["shader_file"].value_or("");
    auto dxil = mShaderCompiler.CompileFromFile("Shaders/" + std::string(file) + ".hlsl");
    assert(dxil != nullptr);
    CD3DX12_SHADER_BYTECODE dxilCode {dxil->GetBufferPointer(), dxil->GetBufferSize()};

    // Create the root signature
    mDXDevice->CreateRootSignature(0, dxilCode.pShaderBytecode, dxilCode.BytecodeLength, IID_PPV_ARGS(&mRootSig));

    CD3DX12_STATE_OBJECT_DESC rtPipeline(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    // Add the DXIL library
    auto* lib = rtPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    lib->SetDXILLibrary(&dxilCode);
    lib->DefineExport(L"ShaderConfig");
    lib->DefineExport(L"PipelineConfig");
    lib->DefineExport(L"RootSig");
    lib->DefineExport(L"AABBHitGroup");
    lib->DefineExport(L"rgen");
    lib->DefineExport(L"chit");
    lib->DefineExport(L"isect");
    lib->DefineExport(L"miss");

    // Export the root signature
    mPipeline = mDevice->CreatePipeline(rtPipeline);

    mShaderTable.AddShader(L"rgen", DXR::ShaderType::RayGen);
    mShaderTable.AddShader(L"miss", DXR::ShaderType::Miss);
    mShaderTable.AddShader(L"AABBHitGroup", DXR::ShaderType::HitGroup);

    mDevice->CreateShaderTable(mShaderTable, D3D12_HEAP_TYPE_GPU_UPLOAD, mPipeline);

    // Create AS
    {
        std::string_view scene = config["scene"].value_or("");
        mScene = LoadAsAABBs(mDevice, "Data/" + std::string(scene) + ".vox");
        mBenchmarkFrameCount = config["benchmark_frames"].value_or(UINT16_MAX);
        mPerformanceData.reserve(mBenchmarkFrameCount);

        auto sceneConfig = toml::parse_file("Data/" + std::string(scene) + ".toml");

        mSceneLightIntensity = sceneConfig["Scene"]["light_intensity"].value_or(1.0);
        mSkyBrightness = sceneConfig["Scene"]["sky_brightness"].value_or(1.0);

        mCamera.Position.x = sceneConfig["Camera"]["x"].value_or(0.0);
        mCamera.Position.y = sceneConfig["Camera"]["y"].value_or(0.0);
        mCamera.Position.z = sceneConfig["Camera"]["z"].value_or(0.0);
        mCamera.Fov = sceneConfig["Camera"]["fov"].value_or(45.0);

        float pitch = sceneConfig["Camera"]["pitch"].value_or(0.0);
        float yaw = sceneConfig["Camera"]["yaw"].value_or(0.0);
        float roll = sceneConfig["Camera"]["roll"].value_or(0.0);
        mCamera.SetRotation(pitch, yaw, roll);
        mCamera.Speed = sceneConfig["Camera"]["speed"].value_or(25.0);

        // Performance File
        // Write the header to the file
        mPerformanceFile.open(std::string(scene) + "-" + std::string(file) + ".csv", std::ios::out);
        mPerformanceFile << "Frame,FrameTime" << std::endl;
    }

    std::cout << "Number of Voxels: " << mScene->NumVoxels << std::endl;
    std::cout << "Acceleration Structure Memory Consumption: " << mScene->ASMemoryConsumption << " Bytes" << std::endl;
    std::cout << "Buffers Memory Consumption: " << mScene->BuffersMemoryConsumption << " Bytes" << std::endl;

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

    // Create the output buffer
    auto allocDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT64) * 2);

    // Get the timestamp frequency
    UINT64 frequency;
    THROW_IF_FAILED(mCommandQueue->GetTimestampFrequency(&frequency));
    mTimestampFrequency = frequency;

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mResourceHeap->GetCPUDescriptorHandleForHeapStart());
    cpuHandle.Offset(3, mResourceDescriptorSize);

    // Color buffer
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = 256;
    srvDesc.Buffer.StructureByteStride = sizeof(VoxMaterial);
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

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

    D3D12_DISPATCH_RAYS_DESC desc = mShaderTable.GetRaysDesc(0, mWidth, mHeight);
    mCommandList->DispatchRays(&desc);

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
}

void AxisAlignedIntersection::WritePerformanceData()
{
    for (auto& data : mPerformanceData) { mPerformanceFile << data.Frame << "," << data.FrameTime << std::endl; }
}

void AxisAlignedIntersection::Stop()
{
    // Write the performance data to the file
    WritePerformanceData();
    mPerformanceFile.close();
}

void AxisAlignedIntersection::EndFrame()
{
    Application::EndFrame();

    if (mFrameCount >= mBenchmarkFrameCount)
    {
        glfwSetWindowShouldClose(mWindow, true);
    }

    // Calculate the time taken
    PerformanceData data;
    data.Frame = mFrameCount;
    // First frame doesn't have a delta time
    data.FrameTime = mFrameCount == 1 ? 0.0 : DeltaTime * 1000.0;
    mPerformanceData.push_back(data);
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
