#include "Shaders/Common/Common.hlsl"

[shader("closesthit")]
void chit(inout Payload p, in BoxHitAttributes attribs)
{
    float3 Color = float3(1.0, 1.0, 1.0);

    float3 v = WorldRayDirection();
    float time = otherInfo.x;
	uint seed = asint(time) * asint(v.x) * asint(v.y) * asint(v.z); 

	// Sample a new ray direction
	float2 rand = float2(NextRandomFloat(seed), NextRandomFloat(seed));

    p.HitColor = PrimitiveIndex() / 500000.0;
    p.HitLight = 1.0;
    p.TerminateRay = true;
    p.NextDir = SampleCosineHemisphere(attribs.Normal, rand);
    p.NextOrigin = GetWorldIntersection();
}