#include "Vulray/Vulray.h"
#include "Base/Common.h"
#include "Base/ShaderCompiler.h"
#include "Base/Application.h"
#include "Base/FileRead.h"
#include "Base/MeshLoader.h"

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
    virtual void Update(vk::CommandBuffer renderCmd) override;
    virtual void Stop() override;

    // functions to break up the start function
    void CreateAS();
    void CreateRTPipeline();
    void UpdateDescriptorSet();
    void CreateQueryPool();

public:
    ShaderCompiler mShaderCompiler;

    MeshLoader mMeshLoader;

    vr::AllocatedBuffer mAABBBuffer;
    vr::AllocatedBuffer mTransformBuffer;

    vr::SBTBuffer mSBTBuffer;

    std::vector<vr::DescriptorItem> mResourceBindings;
    vk::DescriptorSetLayout mResourceDescriptorLayout;
    vr::DescriptorBuffer mResourceDescBuffer;

    vk::Pipeline mRTPipeline = nullptr;
    vk::PipelineLayout mPipelineLayout = nullptr;

    std::vector<vr::BLASHandle> mBLASHandles;
    vr::TLASHandle mTLASHandle;

    vk::QueryPool mPerfTimePool = nullptr;
    double mTimestampPeriodToMs = 0.0;

    std::ofstream mPerformanceFile;

    std::vector<PerformanceData> mPerformanceData;
};


void BoxIntersections::Start()
{
    CreateQueryPool();

    CreateBaseResources();

    CreateRTPipeline();

    CreateAS();

    UpdateDescriptorSet();
}

void BoxIntersections::CreateQueryPool()
{

    vk::PhysicalDeviceLimits const& device_limits = mPhysicalDevice.getProperties().limits;

    if (device_limits.timestampPeriod == 0)
    {
        throw std::runtime_error{ "The selected device does not support timestamp queries!" };
    }

    if (!device_limits.timestampComputeAndGraphics)
    {
        // Check if the graphics queue used in this sample supports time stamps
        vk::QueueFamilyProperties const& graphics_queue_family_properties = mPhysicalDevice.getQueueFamilyProperties()[mQueues.GraphicsIndex];
        if (graphics_queue_family_properties.timestampValidBits == 0)
        {
            throw std::runtime_error{ "The selected graphics queue family does not support timestamp queries!" };
        }
    }

    mPerfTimePool = mDevice.createQueryPool(vk::QueryPoolCreateInfo({}, vk::QueryType::eTimestamp, 2));

    mTimestampPeriodToMs = device_limits.timestampPeriod / 1000000.0;

    // Write the header to the file
    mPerformanceFile.open("PerformanceData.csv", std::ios::out);
    mPerformanceFile << "Application Time (s),Performance Time (ms)" << std::endl;

    mPerformanceData.reserve(100);
}

void BoxIntersections::CreateAS()
{
    std::vector<uint8_t> rawVox;
    FileRead("Assets/monu1.vox", rawVox);
    auto voxScene = ogt_vox_read_scene(rawVox.data(), rawVox.size());

    std::vector<glm::vec3> positions = {};
    std::vector<glm::mat4> transforms = {};

    for (uint32_t i = 0; i < voxScene->num_instances; i++)
    {
        auto& instance = voxScene->instances[i];
        auto& model = voxScene->models[instance.model_index];
        
        transforms.push_back(glm::transpose(glm::make_mat4(&instance.transform.m00)));

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
                    // if (model->voxel_data[x + y * sizeX + z * sizeX * sizeY] != 0)
                    // {
                        positions.push_back(glm::vec3(x, z, y));
					// }
				}
			}
		}

    }

    uint64_t sceneSize = positions.size() * sizeof(vk::AabbPositionsKHR);
    uint64_t transformSize = transforms.size() * sizeof(glm::mat4);

    mAABBBuffer = mVRDev->CreateBuffer(
        sceneSize,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); // this buffer will be used as a source for the BLAS

    mTransformBuffer = mVRDev->CreateBuffer(
        transformSize,
        vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); // this buffer will be used as a source for the BLAS

    // Convert Point Cloud to AABBs

    std::vector<vr::BLASCreateInfo> blasCreateInfos = {};

    uint64_t offset = 0;
    std::vector<vk::AabbPositionsKHR> aabbs = {};
    aabbs.reserve(sceneSize / sizeof(vk::AabbPositionsKHR));

    auto& blasInfo = blasCreateInfos.emplace_back();

    auto& geomData = blasInfo.Geometries.emplace_back();
    geomData.Type = vk::GeometryTypeKHR::eAabbs;
    geomData.PrimitiveCount = positions.size();
    geomData.Stride = sizeof(vk::AabbPositionsKHR);
    geomData.DataAddresses.AABBDevAddress = mAABBBuffer.DevAddress;
    geomData.DataAddresses.TransformDevAddress = mTransformBuffer.DevAddress;

    for (auto& pos : positions)
    {
        const float voxSide = 0.5;

		auto aabb = vk::AabbPositionsKHR();
		aabb.minX = pos.x - voxSide;
		aabb.minY = pos.y - voxSide;
		aabb.minZ = pos.z - voxSide;
		aabb.maxX = pos.x + voxSide;
		aabb.maxY = pos.y + voxSide;
		aabb.maxZ = pos.z + voxSide;

		aabbs.push_back(aabb);
	}

    std::vector<vr::BLASBuildInfo> buildInfos = {};

    for (auto& blasCreateInfo : blasCreateInfos)
    {
        auto& buildInfo = buildInfos.emplace_back();
        auto& blasHandle = mBLASHandles.emplace_back();

        std::tie(blasHandle, buildInfo) = mVRDev->CreateBLAS(blasCreateInfo);
	}

    mVRDev->UpdateBuffer(mAABBBuffer, aabbs.data(), sceneSize, 0);
    mVRDev->UpdateBuffer(mTransformBuffer, transforms.data(), transformSize, 0);


    auto BLASscratchBuffer = mVRDev->CreateScratchBufferFromBuildInfos(buildInfos);


    // create a TLAS
    vr::TLASCreateInfo tlasCreateInfo = {};
    tlasCreateInfo.Flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    tlasCreateInfo.MaxInstanceCount = 1; // Max number of instances in the TLAS, when building the TLAS num of instances may be lower

    auto [tlasHandle, tlasBuildInfo] = mVRDev->CreateTLAS(tlasCreateInfo);

    mTLASHandle = tlasHandle;

    auto TLASScratchBuffer = mVRDev->CreateScratchBufferFromBuildInfo(tlasBuildInfo);

    // create a buffer for the instance data
    auto InstanceBuffer = mVRDev->CreateInstanceBuffer(mBLASHandles.size());

    // Specify the instance data

    offset = 0;
    for (auto& blasHandle : mBLASHandles)
    {
		auto inst = vk::AccelerationStructureInstanceKHR()
					.setInstanceCustomIndex(0)
					.setAccelerationStructureReference(blasHandle.Buffer.DevAddress)
					.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque)
					.setMask(0xFF)
					.setInstanceShaderBindingTableRecordOffset(0);

		// set the transform matrix to identity
        inst.transform = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 1.0f,
			0.0f, 0.0f, 1.0f, 0.0f};

		mVRDev->UpdateBuffer(InstanceBuffer, &inst, sizeof(vk::AccelerationStructureInstanceKHR), offset);

		offset += sizeof(vk::AccelerationStructureInstanceKHR);
	}


    auto buildCmd = mDevice.allocateCommandBuffers(vk::CommandBufferAllocateInfo(mGraphicsPool, vk::CommandBufferLevel::ePrimary, 1))[0];

    buildCmd.begin(vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));


    mVRDev->BuildBLAS(buildInfos, buildCmd);

    mVRDev->AddAccelerationBuildBarrier(buildCmd); // Add a barrier to the command buffer to make sure the BLAS build is finished before the TLAS build starts

    mVRDev->BuildTLAS(tlasBuildInfo, InstanceBuffer, 1, buildCmd);

    buildCmd.end();

    // submit the command buffer and wait for it to finish
    auto submitInfo = vk::SubmitInfo()
                          .setCommandBufferCount(1)
                          .setPCommandBuffers(&buildCmd);

    mQueues.GraphicsQueue.submit(submitInfo, nullptr);

    mDevice.waitIdle();

    // Free the scratch buffers
    mVRDev->DestroyBuffer(BLASscratchBuffer);
    mVRDev->DestroyBuffer(TLASScratchBuffer);

    mVRDev->DestroyBuffer(InstanceBuffer);

    mDevice.freeCommandBuffers(mGraphicsPool, buildCmd);
}

void BoxIntersections::CreateRTPipeline()
{

    mResourceBindings = {
        vr::DescriptorItem(0, vk::DescriptorType::eAccelerationStructureKHR, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mTLASHandle.Buffer.DevAddress),
        vr::DescriptorItem(1, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR, 1, &mUniformBuffer),
        vr::DescriptorItem(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR | vk::ShaderStageFlagBits::eIntersectionKHR, 1, &mAABBBuffer),
        vr::DescriptorItem(3, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mOutputImage),
        vr::DescriptorItem(4, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR, 1, &mAccumImage)
    };

    mResourceDescriptorLayout = mVRDev->CreateDescriptorSetLayout(mResourceBindings);

    mPipelineLayout = mVRDev->CreatePipelineLayout(mResourceDescriptorLayout);

    // create shaders for the ray tracing pipeline
    auto spv = mShaderCompiler.CompileSPIRVFromFile("Shaders/BasicShader.hlsl");
    auto chit = mShaderCompiler.CompileSPIRVFromFile("Shaders/Hit.hlsl");
    auto isect = mShaderCompiler.CompileSPIRVFromFile("Shaders/Intersection.hlsl");

    auto shaderModule = mVRDev->CreateShaderFromSPV(spv);
    auto chitModule = mVRDev->CreateShaderFromSPV(chit);
    auto isectModule = mVRDev->CreateShaderFromSPV(isect);

    vr::PipelineSettings pipelineSettings = {};
    pipelineSettings.PipelineLayout = mPipelineLayout;
    pipelineSettings.MaxRecursionDepth = 1;
    pipelineSettings.MaxPayloadSize = sizeof(float) * 2 + sizeof(glm::vec3) * 3;
    pipelineSettings.MaxHitAttributeSize = sizeof(glm::vec3);

    vr::RayTracingShaderCollection shaderCollection = {};
    shaderCollection.RayGenShaders.push_back(shaderModule);
    shaderCollection.RayGenShaders.back().EntryPoint = "rgen";

    shaderCollection.MissShaders.push_back(shaderModule);
    shaderCollection.MissShaders.back().EntryPoint = "miss";

    vr::HitGroup hitGroup = {};
    hitGroup.ClosestHitShader = chitModule;
    hitGroup.ClosestHitShader.EntryPoint = "chit";
    hitGroup.IntersectionShader = isectModule;
    hitGroup.IntersectionShader.EntryPoint = "isect";
    shaderCollection.HitGroups.push_back(hitGroup);

    auto [pipeline, sbtInfo] = mVRDev->CreateRayTracingPipeline(shaderCollection, pipelineSettings);
    mRTPipeline = pipeline;

    mSBTBuffer = mVRDev->CreateSBT(mRTPipeline, sbtInfo);

    // create a descriptor buffer for the ray tracing pipeline
    mResourceDescBuffer = mVRDev->CreateDescriptorBuffer(mResourceDescriptorLayout, mResourceBindings, vr::DescriptorBufferType::Resource);

    mDevice.destroyShaderModule(shaderModule.Module);
    mDevice.destroyShaderModule(chitModule.Module);
    mDevice.destroyShaderModule(isectModule.Module);
}

void BoxIntersections::UpdateDescriptorSet()
{
    mCamera.Position = glm::vec3(0.0f, 0.0f, 5.0f);

    mVRDev->UpdateDescriptorBuffer(mResourceDescBuffer, mResourceBindings, vr::DescriptorBufferType::Resource);
}

void BoxIntersections::Update(vk::CommandBuffer renderCmd)
{
    UpdateCamera();

    renderCmd.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

    renderCmd.resetQueryPool(mPerfTimePool, 0, 2);

    mVRDev->BindDescriptorBuffer({mResourceDescBuffer}, renderCmd);
    mVRDev->BindDescriptorSet(mPipelineLayout, 0, 0, 0, renderCmd);

    mVRDev->TransitionImageLayout(
        mOutputImageBuffer.Image,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eGeneral,
        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
        renderCmd);

    renderCmd.writeTimestamp(vk::PipelineStageFlagBits::eRayTracingShaderKHR, mPerfTimePool, 0);

    mVRDev->DispatchRays(mRTPipeline, mSBTBuffer, mRenderWidth, mRenderHeight, 1, renderCmd);

    renderCmd.writeTimestamp(vk::PipelineStageFlagBits::eRayTracingShaderKHR, mPerfTimePool, 1);

    // Helper function in Application Class to blit the image to the swapchain image
    BlitImage(renderCmd);

    renderCmd.end();

    WaitForRendering();

    Present(renderCmd);

    // Get the timestamps
    uint64_t timestamps[2] = {};
    auto _ = mDevice.getQueryPoolResults(mPerfTimePool, 0, 2, sizeof(uint64_t) * 2, timestamps, sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

    uint64_t time = timestamps[1] - timestamps[0];

    mPerformanceData.push_back({glfwGetTime(), time * mTimestampPeriodToMs});

    // VULRAY_FLOG_INFO("Performance Time: %f", time * mTimestampPeriodToMs);

    if (mPerformanceData.size() > 100)
    {
        for (auto& data : mPerformanceData)
        {
            mPerformanceFile << data.ApplicationTime << "," << data.PerformanceTime << "\n";
		}

        mPerformanceData.clear();
	}

}

void BoxIntersections::Stop()
{
    auto _ = mDevice.waitForFences(mRenderFence, VK_TRUE, UINT64_MAX);

    // destroy all the resources we created
    mVRDev->DestroySBTBuffer(mSBTBuffer);

    mDevice.destroyPipeline(mRTPipeline);
    mDevice.destroyPipelineLayout(mPipelineLayout);

    mDevice.destroyDescriptorSetLayout(mResourceDescriptorLayout);
    mVRDev->DestroyBuffer(mResourceDescBuffer.Buffer);

    mVRDev->DestroyBuffer(mAABBBuffer);
    mVRDev->DestroyBuffer(mTransformBuffer);

    for (auto& blasHandle : mBLASHandles)
    {
		mVRDev->DestroyBLAS(blasHandle);
	}

    mDevice.destroyQueryPool(mPerfTimePool);

    mVRDev->DestroyTLAS(mTLASHandle);

    for (auto& data : mPerformanceData)
    {
        mPerformanceFile << data.ApplicationTime << "," << data.PerformanceTime << "\n";
    }

    mPerformanceData.clear();
}

int main()
{
    // Create the application, start it, run it and stop it, boierplate code, eg initialising vulkan, glfw, etc
    // that is the same for every application is handled by the Application class
    // it can be found in the Base folder
    Application *app = new BoxIntersections();

    app->Start();
    app->Run();
    app->Stop();

    delete app;
}