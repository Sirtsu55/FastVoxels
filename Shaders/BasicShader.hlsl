#include "Shaders/Common/Common.hlsl"

#define MAX_BOUNCES 10

[shader("raygeneration")]
void rgen()
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[0];
    RWTexture2D<float4> outImage = ResourceDescriptorHeap[1];
    RWTexture2D<float4> accumImage = ResourceDescriptorHeap[2];
    RaytracingAccelerationStructure rs = ResourceDescriptorHeap[3];
    
    
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    RayDesc rayDesc = ConstructRay(sceneInfo.View, sceneInfo.Proj);

    Payload payload;
    payload.HitColor = float3(1.0, 1.0, 1.0);

    float3 Radiance = 1.0;

    for (int i = 0; i < MAX_BOUNCES; i++)
    {
        TraceRay(rs, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, rayDesc, payload);
      
        if (payload.TerminateRay)      
            break;

        rayDesc.Origin = payload.HitLoc;
        rayDesc.Direction = payload.NextDir;
    }

    Radiance = payload.HitColor * payload.HitLight;
    
    if (any(isnan(Radiance)))
        Radiance = float3(0.0, 0.0, 0.0);
    
    uint frameCount = asuint(sceneInfo.otherInfo.x);
    
    float3 Accumulated = 0;
    
    if (frameCount == 0)
        Accumulated = Radiance;
    else
        Accumulated = accumImage[int2(LaunchID.xy)].xyz + Radiance;
    
    accumImage[int2(LaunchID.xy)] = float4(Accumulated, 0.0);

    float3 FinalColor = (Accumulated) / float(1 + frameCount);
    
    outImage[int2(LaunchID.xy)] = float4(FinalColor, 0.0);
}

[shader("intersection")]
void isect()
{
    BoxHitAttributes attribs;

    ReportHit(RayTCurrent(), 0, attribs);
}

[shader("closesthit")]
void chit(inout Payload p, in BoxHitAttributes attribs)
{
    ConstantBuffer<SceneInfo> sceneInfo = ResourceDescriptorHeap[0];
    
    float3 Color = float3(1.0, 1.0, 1.0);

    float3 v = WorldRayDirection();
    float time = asfloat(sceneInfo.otherInfo.y);
    uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z);

	// Sample a new ray direction
    float2 rand = float2(NextRandomFloat(seed), NextRandomFloat(seed));

    p.HitColor = RayTCurrent() / 50;
    p.HitLight = 1.0;
    p.TerminateRay = true;
    p.NextDir = SampleCosineHemisphere(attribs.Normal, rand);
    p.HitLoc = GetWorldIntersection();
}

[shader("miss")]
void miss(inout Payload p)
{
    // RT in one weekend sky
    float3 unitDirection = normalize(WorldRayDirection());
    float t = 0.5 * (unitDirection.y + 1.0);
    p.HitColor *= lerp(float3(1.0, 1.0, 1.0), float3(0.5, 0.7, 1.0), t);
    p.HitLight = 1.0f;
    p.TerminateRay = true;
}