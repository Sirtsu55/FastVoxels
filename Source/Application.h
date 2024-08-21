#pragma once
#include <GLFW/glfw3.h>
#include "SimpleTimer.h"
#include "Camera.h"

class Application
{
public:
    Application();
    virtual ~Application();

    // Functions to be overriden by the samples
    virtual void Start() {};

    virtual void Update() {};

    virtual void Stop() {};

    virtual void BeginFrame();

    virtual void EndFrame();

    void HandleIO();

    void CleanUp();

    void Run();

private:
protected:
    ComPtr<IDXGIFactory7> mFactory = nullptr;
    ComPtr<IDXGIAdapter4> mAdapter = nullptr;
    ComPtr<IDXGISwapChain4> mSwapchain = nullptr;

    ComPtr<ID3D12Device7> mDXDevice = nullptr;
    ComPtr<ID3D12CommandQueue> mCommandQueue = nullptr;
    ComPtr<ID3D12CommandAllocator> mCommandAllocators[2] = {0};
    ComPtr<ID3D12GraphicsCommandList4> mCommandList = nullptr;

    ComPtr<ID3D12DescriptorHeap> mRTVHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> mResourceHeap = nullptr;
    ComPtr<ID3D12Fence> mFence = nullptr;
    ComPtr<ID3D12Resource> mBackBuffers[2] = {0};

    UINT64 mFenceValue = 0;

    UINT64 mFrameFenceValues[2] = {0};

    HANDLE mFenceEvent = nullptr;

    std::shared_ptr<DXR::Device> mDevice = nullptr;

    UINT32 mTearingSupport = 0;
    UINT32 mBackBufferIndex = 0;
    UINT32 mRTVDescriptorSize = 0;
    UINT32 mResourceDescriptorSize = 0;

    // User descriptor heap starts
    // 0 - Scene constant buffer
    // 1 - Output image
    // 2 - Accumulation image
    // 3... - User defined
    constexpr inline static UINT32 UserDescriptorStartIndex = 3;

    ComPtr<DMA::Allocation> mOutputImage;
    ComPtr<DMA::Allocation> mAccumulationImage;
    ComPtr<DMA::Allocation> mConstantBuffer;
    ComPtr<DMA::Allocation> mStagingBuffers[2] = {0};
    CHAR* mStagingDatas[2] = {0};

    GLFWwindow* mWindow = nullptr;

    UINT32 mWidth = 1980;
    UINT32 mHeight = 1080;

    Camera mCamera;
    float mSceneLightIntensity = 1.0f;
    float mSkyBrightness = 1.0f;

    glm::dvec2 mMousePos = {0.0f, 0.0f};
    glm::dvec2 mMouseDelta = {0.0f, 0.0f};

    float MouseSensitivity = 2.0f;
    float MovementSpeed = 1.0f;

    float DeltaTime = 0.0f;

    UINT64 mFrameCount = 0;
    UINT64 mPassiveFrameCount = 0;

    SimpleTimer mFrameTimer;
};
