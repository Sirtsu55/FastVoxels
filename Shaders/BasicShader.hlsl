#include "Shaders/Common/Common.hlsl"

#define MAX_BOUNCES 10

[shader("raygeneration")]
void rgen()
{
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    RayDesc rayDesc = ConstructRay(viewInverse, projInverse);

    Payload payload;
    payload.HitColor = float3(1.0, 1.0, 1.0);

    float3 Radiance = 1.0;

    for (int i = 0; i < MAX_BOUNCES; i++)
    {
        TraceRay(rs, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, rayDesc, payload);
      
        if (payload.TerminateRay)      
            break;

        rayDesc.Origin = payload.NextOrigin;
        rayDesc.Direction = payload.NextDir;
    }

    Radiance = payload.HitColor * payload.HitLight;
    if (any(isnan(Radiance)))
        Radiance = float3(0.0, 0.0, 0.0);
    
    uint frameCount = asuint(otherInfo.y);
    
    float3 Accumulated = 0;
    
    if (frameCount == 0)
        Accumulated = Radiance;
    else
        Accumulated = accumImage[int2(LaunchID.xy)].xyz + Radiance;
    
    accumImage[int2(LaunchID.xy)] = float4(Accumulated, 0.0);

    float3 FinalColor = (Accumulated) / float(1 + frameCount);
    
    FinalColor = clamp(FinalColor, 0.0, 1.0);
    
    outImage[int2(LaunchID.xy)] = float4(FinalColor, 0.0);
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