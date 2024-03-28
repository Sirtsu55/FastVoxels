#include "Shaders/Common/Common.hlsl"

#define MAX_BOUNCES 5

[shader("raygeneration")]
void rgen()
{
    const uint3 LaunchID = DispatchRaysIndex();
    const uint3 LaunchSize = DispatchRaysDimensions();

    RayDesc rayDesc = ConstructRay(viewInverse, projInverse);

    Payload payload;
    payload.HitColor = float3(1.0, 1.0, 1.0);

    float3 Radiance = 1.0f;

    for (int i = 0; i < MAX_BOUNCES; i++)
    {
        TraceRay(rs, RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xff, 0, 0, 0, rayDesc, payload);
      
        if (payload.TerminateRay)      
            break;

        rayDesc.Origin = payload.NextOrigin;
        rayDesc.Direction = payload.NextDir;
    }

    Radiance = payload.HitColor * payload.HitLight;

    uint frameCount = asuint(otherInfo.y);

    float3 Accumulated = accumImage[int2(LaunchID.xy)].xyz;

    float3 FinalColor = (Radiance + Accumulated) / float(1 + frameCount);

    outImage[int2(LaunchID.xy)] = float4(FinalColor, 0.0);

    // Reset the accumulation buffer
	if (frameCount == 0)
		accumImage[int2(LaunchID.xy)] = float4(Radiance, 0.0);
	else
		accumImage[int2(LaunchID.xy)] = float4(Radiance + Accumulated, 0.0);
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