#include "Common.h"
#include "Application.h"

Application::Application()
{
    glfwInit(); // Initializes the GLFW library

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    mWindow = glfwCreateWindow(mWidth, mHeight, "FastVoxels", nullptr, nullptr); // Creates a window

    // Create Factory
    uint32_t dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    THROW_IF_FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&mFactory)));

#if defined(_DEBUG)
    // Enable the D3D12 debug layer.
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
    }
#endif

    // Check for tearing support
    mFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &mTearingSupport, sizeof(UINT32));

    // Create Adapter
    mFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&mAdapter));

    // Create Device

    if (FAILED(D3D12CreateDevice(mAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&mDXDevice))))
    {
        MessageBoxW(NULL, L"Failed to create device", L"Error",
                    MB_OK); // Displays a message box with the title "Error" and the message "Failed to create device
        std::exit(1);
    }

    CD3DX12FeatureSupport featureSupport;
    featureSupport.Init(mDXDevice.Get());

    if (!featureSupport.GPUUploadHeapSupported())
    {
        MessageBoxW(NULL, L"GPU Upload Heap is not supported on this device", L"Error", MB_OK);
        std::exit(1);
    }

    if (featureSupport.RaytracingTier() == D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
    {
        MessageBoxW(NULL, L"Raytracing is not supported on this device", L"Error", MB_OK);
        std::exit(1);
    }

    // Enable Info Queue
#if defined(_DEBUG)
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(mDXDevice.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO,
        };

        // D3D12_MESSAGE_ID denyIds[] = {
        // };

        D3D12_INFO_QUEUE_FILTER newFilter = {};
        newFilter.DenyList.NumSeverities = _countof(severities);
        newFilter.DenyList.pSeverityList = severities;
        // newFilter.DenyList.NumIDs = _countof(denyIds);
        // newFilter.DenyList.pIDList = denyIds;

        THROW_IF_FAILED(pInfoQueue->PushStorageFilter(&newFilter));
    }
#endif

    // Create Command Queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    THROW_IF_FAILED(mDXDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));

    // Create Swap Chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Width = 0;
    swapChainDesc.Height = 0;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = mTearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain1 = nullptr;

    HWND hwnd = glfwGetWin32Window(mWindow);

    THROW_IF_FAILED(
        mFactory->CreateSwapChainForHwnd(mCommandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain1));
    THROW_IF_FAILED(mFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

    swapChain1.As(&mSwapchain);

    mBackBufferIndex = mSwapchain->GetCurrentBackBufferIndex();

    // Create Command Allocators
    for (UINT32 i = 0; i < 2; i++)
    {
        THROW_IF_FAILED(
            mDXDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[i])));
    }

    // Create Command List
    THROW_IF_FAILED(mDXDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE,
                                                  IID_PPV_ARGS(&mCommandList)));

    // Create RTV Descriptor Heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 2;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    THROW_IF_FAILED(mDXDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRTVHeap)));

    mRTVDescriptorSize = mDXDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(mRTVHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT32 i = 0; i < 2; i++)
    {
        THROW_IF_FAILED(mSwapchain->GetBuffer(i, IID_PPV_ARGS(&mBackBuffers[i])));
        mDXDevice->CreateRenderTargetView(mBackBuffers[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, mRTVDescriptorSize);
    }

    // Create Fence
    THROW_IF_FAILED(mDXDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));
    mFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (mFenceEvent == nullptr)
    {
        THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
    }

    mDevice = std::make_unique<DXR::Device>(mDXDevice, mAdapter);

    CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, mWidth, mHeight, 1, 1, 1, 0,
                                                              D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    mOutputImage = mDevice->AllocateResource(desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    mAccumulationImage =
        mDevice->AllocateResource(desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT);

    desc = CD3DX12_RESOURCE_DESC::Buffer(1024, D3D12_RESOURCE_FLAG_NONE);

    mConstantBuffer = mDevice->AllocateResource(desc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_DEFAULT);
    mStagingBuffers[0] = mDevice->AllocateResource(desc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_UPLOAD);
    mStagingBuffers[1] = mDevice->AllocateResource(desc, D3D12_RESOURCE_STATE_COMMON, D3D12_HEAP_TYPE_UPLOAD);
    mStagingDatas[0] = (CHAR*)mDevice->MapAllocationForWrite(mStagingBuffers[0]);
    mStagingDatas[1] = (CHAR*)mDevice->MapAllocationForWrite(mStagingBuffers[1]);

    // Create Resource Heap
    D3D12_DESCRIPTOR_HEAP_DESC resourceHeapDesc = {};
    resourceHeapDesc.NodeMask = 0;
    resourceHeapDesc.NumDescriptors = 1'000'000;
    resourceHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    resourceHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    THROW_IF_FAILED(mDXDevice->CreateDescriptorHeap(&resourceHeapDesc, IID_PPV_ARGS(&mResourceHeap)));
    mResourceDescriptorSize = mDXDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mResourceHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = mConstantBuffer->GetResource()->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = 1024;
    mDXDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);

    cpuHandle.Offset(1, mResourceDescriptorSize);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavDesc.Texture2D.MipSlice = 0;
    uavDesc.Texture2D.PlaneSlice = 0;
    mDXDevice->CreateUnorderedAccessView(mOutputImage->GetResource(), nullptr, &uavDesc, cpuHandle);

    cpuHandle.Offset(1, mResourceDescriptorSize);

    uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    mDXDevice->CreateUnorderedAccessView(mAccumulationImage->GetResource(), nullptr, &uavDesc, cpuHandle);
}

void Application::BeginFrame()
{
    // Acquire the next image
    mBackBufferIndex = mSwapchain->GetCurrentBackBufferIndex();

    // Wait for the previous frame to finish
    if (mFence->GetCompletedValue() < mFrameFenceValues[mBackBufferIndex])
    {
        THROW_IF_FAILED(mFence->SetEventOnCompletion(mFrameFenceValues[mBackBufferIndex], mFenceEvent));
        WaitForSingleObject(mFenceEvent, INFINITE);
    }

    UpdateCamera();

    DeltaTime = mFrameTimer.Endd();
    mFrameTimer.Start();

    // Reset the command allocator
    THROW_IF_FAILED(mCommandAllocators[mBackBufferIndex]->Reset());

    // Reset the command list
    THROW_IF_FAILED(mCommandList->Reset(mCommandAllocators[mBackBufferIndex].Get(), nullptr));

    // Copy the data to the constant buffer
    mCommandList->CopyBufferRegion(mConstantBuffer->GetResource(), 0, mStagingBuffers[mBackBufferIndex]->GetResource(),
                                   0, 1024);

    ID3D12DescriptorHeap* ppHeaps[] = {mResourceHeap.Get()};
    mCommandList->SetDescriptorHeaps(1, ppHeaps);
}

void Application::EndFrame()
{
    mPassiveFrameCount++;
    mFrameCount++;

    // Close the command list
    THROW_IF_FAILED(mCommandList->Close());

    // Execute the command list
    ID3D12CommandList* ppCommandLists[] = {mCommandList.Get()};
    mCommandQueue->ExecuteCommandLists(1, ppCommandLists);

    // Present the image
    THROW_IF_FAILED(mSwapchain->Present(0, mTearingSupport ? DXGI_PRESENT_ALLOW_TEARING : 0));

    // Signal the fence
    THROW_IF_FAILED(mCommandQueue->Signal(mFence.Get(), ++mFenceValue));

    // Store the fence value to wait for
    mFrameFenceValues[mBackBufferIndex] = mFenceValue;
}

void Application::UpdateCamera()
{
    glm::dvec2 lastMousePos = mMousePos;

    glfwGetCursorPos(mWindow, &mMousePos.x, &mMousePos.y);

    // normalize the mouse position
    glm::dvec2 resDividend = glm::dvec2(mWidth, mHeight);
    glm::dvec2 delta = (mMousePos - lastMousePos) / resDividend;

    if (glfwGetMouseButton(mWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        if (delta.x || delta.y)
        {
            mCamera.Rotate(delta.y * DeltaTime * MouseSensitivity, delta.x * DeltaTime * MouseSensitivity, 0);
            mPassiveFrameCount = 0;
        }
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

    mCamera.AspectRatio = ((float)mWidth) / ((float)mHeight);
    glm::mat4 proj = mCamera.GetProjectionMatrix();

    // update the view matrix
    glm::mat4 mats[2] = {glm::inverse(view), glm::inverse(proj)};

    float time = glfwGetTime();

    uint32_t uniformExtraInfo[4];
    uniformExtraInfo[0] = mPassiveFrameCount;
    uniformExtraInfo[1] = *(uint32_t*)&time; // type punning

    CHAR* data = mStagingDatas[mBackBufferIndex];

    memcpy(data, mats, sizeof(mats));
    memcpy(data + sizeof(mats), uniformExtraInfo, sizeof(uniformExtraInfo));
}

void Application::CleanUp()
{
    // Wait for the GPU to finish
    mCommandQueue->Signal(mFence.Get(), ++mFenceValue);
    mFence->SetEventOnCompletion(mFenceValue, mFenceEvent);
    WaitForSingleObject(mFenceEvent, INFINITE);
}

void Application::Run()
{
    Start();

    while (!glfwWindowShouldClose(mWindow))
    {
        BeginFrame();
        Update();
        glfwPollEvents();
        EndFrame();
    }

    CleanUp();

    Stop();
}

Application::~Application()
{
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}
