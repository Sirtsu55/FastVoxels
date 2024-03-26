

struct BoxHitAttributes
{
    float3 Normal;
};

struct [raypayload] Payload
{
    [[vk::location(0)]] float3 HitColor : write(miss, closesthit) : read(caller);
};
