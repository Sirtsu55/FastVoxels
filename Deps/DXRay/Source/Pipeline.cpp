#include "DXRay/Device.h"

namespace DXR
{
    ComPtr<ID3D12StateObject> Device::CreatePipeline(CD3DX12_STATE_OBJECT_DESC& desc)
    {
        ComPtr<ID3D12StateObject> pipeline;
        DXR_THROW_FAILED(mDevice->CreateStateObject(desc, IID_PPV_ARGS(&pipeline)));

        return pipeline;
    }

    ComPtr<ID3D12StateObject> Device::ExpandPipeline(CD3DX12_STATE_OBJECT_DESC& desc,
                                                     ComPtr<ID3D12StateObject>& pipeline,
                                                     ComPtr<ID3D12StateObject>& collection)
    {
        ComPtr<ID3D12StateObject> expandedPipeline;
        DXR_THROW_FAILED(mDevice->AddToStateObject(desc, collection.Get(), IID_PPV_ARGS(&expandedPipeline)));
        return expandedPipeline;
    }

} // namespace DXR
