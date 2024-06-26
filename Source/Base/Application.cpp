#include "Common.h"
#include "Application.h"

Application::Application()
{
    glfwInit(); // Initializes the GLFW library

    if (glfwVulkanSupported() != GLFW_TRUE)
    {
        VULRAY_LOG_ERROR("Vulkan is not supported on this system");
        throw std::runtime_error("Vulkan is not supported on this system");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Sets the client API to GLFW_NO_API, which means that the application will not create an OpenGL context
    
    // Need to enable this so the performance data is consistent across different runs and PCs 
    // so that the user cannot cheat by running this on lower resolution
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "FastVoxels", nullptr, nullptr); // Creates a window

    // specify debug callback by passing a pointer to the function if you want to use it
    // vr::LogCallback = logcback;

    vr::VulkanBuilder builder;
#ifndef NDEBUG
    builder.EnableDebug = true;
#else
    builder.EnableDebug = false;
#endif

    // Get the required extensions
    uint32_t count;
    const char **extensions = glfwGetRequiredInstanceExtensions(&count);
    // Add the extensions to the builder
    for (uint32_t i = 0; i < count; i++)
    {
        builder.InstanceExtensions.push_back(extensions[i]);
    }

#ifdef NDEBUG
    builder.EnableDebug = false;
#else
    builder.EnableDebug = true;
#endif

    // Create the instance
    mInstance = builder.CreateInstance();

    // user can also add extra extensions to the device if they want to use them
    // builder.DeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    // user can also enable extra features if they want to use them
    // features from 1.0 to 1.3 are available
    // builder.PhysicalDeviceFeatures12.bufferDeviceAddress = true;

    // Create the surface for the window
    VkSurfaceKHR surface;
    auto r = glfwCreateWindowSurface(mInstance.InstanceHandle, mWindow, nullptr, &surface);

    mSurface = surface;

    // Pick the physical device to use
    builder.PhysicalDeviceFeatures12.scalarBlockLayout = true;
    builder.PhysicalDeviceFeatures12.hostQueryReset = true;
    mPhysicalDevice = builder.PickPhysicalDevice(mSurface);

    // Create the logical device
    mDevice = builder.CreateDevice();

    // Get the queues for the logical device
    mQueues = builder.GetQueues();

    assert(mQueues.GraphicsQueue && "Graphics queue is null");

    // This code creates a swapchain with a particular format and dimensions.

    mSwapchainBuilder = vr::SwapchainBuilder(mDevice, mPhysicalDevice, mSurface, mQueues.GraphicsIndex, mQueues.PresentIndex);
    mSwapchainBuilder.Height = mWindowWidth;
    mSwapchainBuilder.Width = mWindowHeight;
    mSwapchainBuilder.BackBufferCount = 2;
    mSwapchainBuilder.ImageUsage = vk::ImageUsageFlagBits::eTransferDst;
    mSwapchainBuilder.DesiredFormat = vk::Format::eB8G8R8A8Unorm;
    mSwapchainResources = mSwapchainBuilder.BuildSwapchain();

    // Create command pools
    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.queueFamilyIndex = mQueues.GraphicsIndex;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // release command buffers back to pool
    mGraphicsPool = mDevice.createCommandPool(poolInfo);

    mMaxFramesInFlight = static_cast<uint32_t>(mSwapchainResources.SwapchainImageViews.size());

    // create command buffers
    vk::CommandBufferAllocateInfo allocInfo = {};
    allocInfo.commandPool = mGraphicsPool;
    allocInfo.level = vk::CommandBufferLevel::ePrimary;
    allocInfo.commandBufferCount = 2;

    // Create semaphores
    vk::SemaphoreCreateInfo semaphoreInfo = {};
    // create semaphores for present
    mRenderSemaphore = mDevice.createSemaphore(semaphoreInfo);
    mPresentSemaphore = mDevice.createSemaphore(semaphoreInfo);

    // Create fences
    vk::FenceCreateInfo fenceInfo = {};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
    mRenderFence = mDevice.createFence(fenceInfo);

    mVRDev = new vr::VulrayDevice(mInstance.InstanceHandle, mDevice, mPhysicalDevice);

    mRTRenderCmd = mDevice.allocateCommandBuffers(allocInfo);
}

void Application::Update(vk::CommandBuffer renderCmd)
{
}

void Application::Start()
{
}

void Application::Stop()
{
}

void Application::BeginFrame()
{

    DeltaTime = mFrameTimer.Endd();
    mFrameTimer.Start();
    // Acquire the next image
    auto result = mDevice.acquireNextImageKHR(mSwapchainResources.SwapchainHandle, UINT64_MAX, mRenderSemaphore, nullptr, &mCurrentSwapchainImage);

    if (mOldSwapchain)
    {
        mDevice.destroySwapchainKHR(mOldSwapchain);
        mOldSwapchain = nullptr;
    }
}

void Application::WaitForRendering()
{

}

void Application::Present(vk::CommandBuffer commandBuffer)
{
    // Wait for the fence to be signaled by the GPU
    auto _ = mDevice.waitForFences(mRenderFence, true, 1000000000);
    // Reset the fence
    mDevice.resetFences(mRenderFence);

    // reset the command buffer
    uint32_t lastCmdIdx = mRTRenderCmdIndex == 0 ? 1 : 0;
    mRTRenderCmd[lastCmdIdx].reset(vk::CommandBufferResetFlagBits::eReleaseResources);

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    auto qSubmitInfo = vk::SubmitInfo()
                           .setPWaitDstStageMask(&waitStage)
                           .setCommandBufferCount(1)
                           .setPCommandBuffers(&commandBuffer)
                           .setWaitSemaphoreCount(1)
                           .setPWaitSemaphores(&mRenderSemaphore)
                           .setSignalSemaphoreCount(1)
                           .setPSignalSemaphores(&mPresentSemaphore);

    _ = mQueues.GraphicsQueue.submit(1, &qSubmitInfo, mRenderFence);

    auto presentInfo = vk::PresentInfoKHR()
                           .setWaitSemaphoreCount(1)
                           .setPWaitSemaphores(&mPresentSemaphore)
                           .setSwapchainCount(1)
                           .setPSwapchains(&mSwapchainResources.SwapchainHandle)
                           .setPImageIndices(&mCurrentSwapchainImage);

    // Pass a pointer, not a reference, because Vulkan-hpp EnhancedMode is on, which throws an error if result is not vk::Result::eSuccess
    // the results can be vk::Result::eSuccess, vk::Result::eSuboptimalKHR or vk::Result::eErrorOutOfDateKHR
    auto result = mQueues.PresentQueue.presentKHR(&presentInfo);

    mQueues.GraphicsQueue.waitIdle();

    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        HandleResize();
    }
    else if (result == vk::Result::eSuboptimalKHR)
    {
        HandleResize();
    }
    else if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Unknown result when presenting swapchain: " + vk::to_string(result));
    }
    else
    {
        mPassiveFrameCount++;
        mFrameCount++;
    }

    // new image index
    mRTRenderCmdIndex = mRTRenderCmdIndex == 0 ? 1 : 0;


}

void Application::CreateBaseResources()
{
    // Create an image to render to
    auto imageCreateInfo = vk::ImageCreateInfo()
                               .setImageType(vk::ImageType::e2D)
                               .setFormat(vk::Format::eR32G32B32A32Sfloat)
                               .setExtent(vk::Extent3D(mSwapchainResources.SwapchainExtent, 1))
                               .setMipLevels(1)
                               .setArrayLayers(1)
                               .setSamples(vk::SampleCountFlagBits::e1)
                               .setTiling(vk::ImageTiling::eOptimal)
                               .setUsage(vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc)
                               .setSharingMode(vk::SharingMode::eExclusive)
                               .setInitialLayout(vk::ImageLayout::eUndefined);

    // create the image with dedicated memory
    mOutputImageBuffer = mVRDev->CreateImage(imageCreateInfo, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);
    mAccumImageBuffer = mVRDev->CreateImage(imageCreateInfo, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT);

    // create a view for the image
    auto viewCreateInfo = vk::ImageViewCreateInfo()
                              .setImage(mOutputImageBuffer.Image)
                              .setViewType(vk::ImageViewType::e2D)
                              .setFormat(vk::Format::eR32G32B32A32Sfloat)
                              .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

    mOutputImage.View = mDevice.createImageView(viewCreateInfo);
    viewCreateInfo.setImage(mAccumImageBuffer.Image);
    mAccumImage.View = mDevice.createImageView(viewCreateInfo);

    // create a uniform buffer
    uint32_t uniformBufferSize = sizeof(float) * 4 * 4 * 2; // two 4x4 matrix
    uniformBufferSize += sizeof(float) * 4;                 // pass time, and 3 floats for padding or whatever else in the future

    mUniformBuffer = mVRDev->CreateBuffer(
        uniformBufferSize,
        vk::BufferUsageFlagBits::eUniformBuffer,                 // its a uniform buffer
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT); // we will be writing to this buffer on the CPU
}

void Application::UpdateCamera()
{
    glm::dvec2 lastMousePos = mMousePos;

    glfwGetCursorPos(mWindow, &mMousePos.x, &mMousePos.y);

    // normalize the mouse position
    glm::dvec2 resDividend = glm::dvec2(mSwapchainResources.SwapchainExtent.width, mSwapchainResources.SwapchainExtent.height);
    glm::dvec2 delta = (mMousePos - lastMousePos) / resDividend;

    if (glfwGetMouseButton(mWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (delta.x || delta.y)
        {
            mCamera.Rotate(delta.y * DeltaTime * MouseSensitivity, delta.x * DeltaTime * MouseSensitivity, 0);
            mPassiveFrameCount = 0;
        }
    }
    if (glfwGetKey(mWindow, GLFW_KEY_Q) == GLFW_PRESS)
    {
        mCamera.Rotate(0, 0, DeltaTime);
        mPassiveFrameCount = 0;
    }
    if (glfwGetKey(mWindow, GLFW_KEY_E) == GLFW_PRESS)
    {
        mCamera.Rotate(0, 0, -DeltaTime);
        mPassiveFrameCount = 0;
    }
    if (glfwGetKey(mWindow, GLFW_KEY_W) == GLFW_PRESS)
    {
        mCamera.MoveForward(DeltaTime * MovementSpeed);
        mPassiveFrameCount = 0;
    }
    if (glfwGetKey(mWindow, GLFW_KEY_S) == GLFW_PRESS)
    {
        mCamera.MoveForward(-DeltaTime * MovementSpeed);
        mPassiveFrameCount = 0;
    }
    if (glfwGetKey(mWindow, GLFW_KEY_D) == GLFW_PRESS)
    {
        mCamera.MoveRight(DeltaTime * MovementSpeed);
        mPassiveFrameCount = 0;
    }
    if (glfwGetKey(mWindow, GLFW_KEY_A) == GLFW_PRESS)
    {
        mCamera.MoveRight(-DeltaTime * MovementSpeed);
        mPassiveFrameCount = 0;
    }

    glm::mat4 view = mCamera.GetViewMatrix();

    mCamera.AspectRatio = ((float)mRenderWidth) / ((float)mRenderHeight);
    glm::mat4 proj = mCamera.GetProjectionMatrix();

    // update the view matrix
    glm::mat4 mats[2] = {glm::inverse(view), glm::inverse(proj)};

    float time = glfwGetTime();

    uint32_t uniformExtraInfo[4];
    uniformExtraInfo[0] = *(uint32_t *)&time; // type punning
    uniformExtraInfo[1] = mPassiveFrameCount;

    char *mapped = (char *)mVRDev->MapBuffer(mUniformBuffer);
    memcpy(mapped, mats, sizeof(glm::mat4) * 2);
    memcpy(mapped + sizeof(glm::mat4) * 2, uniformExtraInfo, sizeof(uint32_t) * 4);
    mVRDev->UnmapBuffer(mUniformBuffer);
}

void Application::HandleResize()
{
    // save the old swapchain, because we need to destroy it later after all operations on it are finished
    mOldSwapchain = mSwapchainResources.SwapchainHandle;
    // Destroy the old swapchain resources, but not the swapchain itself
    vr::SwapchainBuilder::DestroySwapchainResources(mDevice, mSwapchainResources);

    mSwapchainResources = mSwapchainBuilder.BuildSwapchain(mOldSwapchain);

    // get the new framebuffer size
    int width, height;
    glfwGetFramebufferSize(mWindow, &width, &height);
    mWindowWidth = width;
    mWindowHeight = height;

    // update the camera aspect ratio
    mCamera.AspectRatio = (float)mRenderWidth / (float)mRenderHeight;

    mPassiveFrameCount = 0;
}

void Application::BlitImage(vk::CommandBuffer renderCmd)
{
    mVRDev->TransitionImageLayout(mSwapchainResources.SwapchainImages[mCurrentSwapchainImage],
                                  vk::ImageLayout::eUndefined,
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
                                  renderCmd);

    mVRDev->TransitionImageLayout(mOutputImageBuffer.Image,
                                  vk::ImageLayout::eGeneral,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
                                  renderCmd);

    renderCmd.blitImage(
        mOutputImageBuffer.Image, vk::ImageLayout::eTransferSrcOptimal,
        mSwapchainResources.SwapchainImages[mCurrentSwapchainImage], vk::ImageLayout::eTransferDstOptimal,
        vk::ImageBlit(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                      {vk::Offset3D(0, 0, 0), vk::Offset3D(mOutputImageBuffer.Width, mOutputImageBuffer.Height, 1)},
                      vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
                      {vk::Offset3D(0, 0, 0), vk::Offset3D(mSwapchainResources.SwapchainExtent.width, mSwapchainResources.SwapchainExtent.height, 1)}),
        vk::Filter::eLinear);

    mVRDev->TransitionImageLayout(mOutputImageBuffer.Image,
                                  vk::ImageLayout::eTransferSrcOptimal,
                                  vk::ImageLayout::eGeneral,
                                  vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
                                  renderCmd);

    mVRDev->TransitionImageLayout(mSwapchainResources.SwapchainImages[mCurrentSwapchainImage],
                                  vk::ImageLayout::eTransferDstOptimal,
                                  vk::ImageLayout::ePresentSrcKHR,
                                  vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1),
                                  renderCmd);
}
void Application::Run()
{
    while (!glfwWindowShouldClose(mWindow))
    {
        BeginFrame();
        Update(mRTRenderCmd[mRTRenderCmdIndex]);
        glfwPollEvents();
    }
}

Application::~Application()
{
    if (mUniformBuffer.Buffer)
        mVRDev->DestroyBuffer(mUniformBuffer);
    if (mOutputImageBuffer.Image)
        mVRDev->DestroyImage(mOutputImageBuffer);
    if (mAccumImageBuffer.Image)
        mVRDev->DestroyImage(mAccumImageBuffer);

    // Clean up
    delete mVRDev;

    glfwDestroyWindow(mWindow);
    glfwTerminate();

    mDevice.destroyImageView(mOutputImage.View);
    mDevice.destroyImageView(mAccumImage.View);

    mDevice.destroyFence(mRenderFence);
    mDevice.destroySemaphore(mRenderSemaphore);
    mDevice.destroySemaphore(mPresentSemaphore);
    mDevice.destroyCommandPool(mGraphicsPool);

    vr::SwapchainBuilder::DestroySwapchain(mDevice, mSwapchainResources);
    mDevice.destroy();
    mInstance.InstanceHandle.destroySurfaceKHR(mSurface);

    vr::InstanceWrapper::DestroyInstance(mInstance);
}