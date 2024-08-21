#pragma once
#include "Shaders/Common/Random.hlsl"

RayDesc ConstructRay(in float4x4 vInv, in float4x4 pInv, uint seed)
{
    //const float2 pixelCenter = float2(DispatchRaysIndex().xy) + float2(0.5, 0.5);
    const float2 pixelCenter = float2(DispatchRaysIndex().xy) + float2(NextRandomFloat(seed), NextRandomFloat(seed));
    
    const float2 inUV = pixelCenter / float2(DispatchRaysDimensions().xy);
    float2 d = inUV * 2.0 - 1.0;
    // Apply jitter
    float4 target = mul(pInv, float4(d.x, d.y, 1, 1));

    RayDesc rayDesc;
	rayDesc.Origin = mul(vInv, float4(0,0,0,1)).xyz;
	rayDesc.Direction = normalize(mul(vInv, float4(normalize(target.xyz), 0)).xyz);
	rayDesc.TMin = 0.1;
	rayDesc.TMax = 10000.0;
    return rayDesc;
}