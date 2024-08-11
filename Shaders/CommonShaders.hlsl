#include "Shaders/Common/Common.hlsl"

[shader("raygeneration")]
void rgen()
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[SceneConstantsIndex];
    RWTexture2D<float4> outImage = ResourceDescriptorHeap[OutputBufferIndex];
    RWTexture2D<float4> accumImage = ResourceDescriptorHeap[AccumulationBufferIndex];
    
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    uint seed = asuint(LaunchID.x) * asuint(LaunchID.y) * asuint(sceneInfo.otherInfo.y);
    RayDesc rayDesc = ConstructRay(sceneInfo.View, sceneInfo.Proj, seed);

    Payload p;
    p.HitColor = 1;
    
    for (uint i = 0; i < 4; i++)
    {
        TraceRay(rs, RAY_FLAG_FORCE_OPAQUE, 0xff, 0, 0, 0, rayDesc, p);
        if (p.T == -1.0f)
            break;
        
        rayDesc.Origin = rayDesc.Origin + p.T * rayDesc.Direction;
        rayDesc.Direction = normalize(p.RayDirection);
    }
    
    const int2 index = int2(LaunchID.xy);
    float3 Radiance = p.HitColor * p.Emission;
    
    if (any(isnan(Radiance)) || any(isinf(Radiance)))
        Radiance = float3(0.0, 0.0, 0.0);
    
    uint frameCount = asuint(sceneInfo.otherInfo.x);
    float4 accum = accumImage[index];
    accum.rgb = frameCount == 0 ? Radiance : accum.rgb + Radiance;
    
    accumImage[index] = accum;
    outImage[index] = accum / float(frameCount + 1);
}

[shader("miss")]
void miss(inout Payload p)
{
    p.HitColor *= float3(0.0, 0.0, 0.0);
    p.Emission = 1.0;
    p.T = -1.0f;
    
    //p.HitColor = float3(0.0, 0.0, 0.0);
    //p.Emission = 0.0;
    //p.T = -1.0f;
    
}