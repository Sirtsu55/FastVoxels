
#define PI 3.1415926535897932384626433832795
#define TWO_PI 6.283185307179586476925286766559
#define EULER_E 2.7182818284590452353602874713527
#define SQRT_OF_ONE_THIRD 0.57735026919

uint Random(uint state)
{
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

uint NextRandomInt(inout uint seed)
{
    seed = Random(seed);
    return seed;
}
float NextRandomFloat(inout uint seed)
{
    seed = Random(seed);
    return seed / (float)0xffffffff;
}


float3 RandomDirection(inout uint seed)
{
    float z = 1.0 - 2.0 * NextRandomFloat(seed);
    float r = sqrt(max(0.0, 1.0 - z * z));
    float phi = TWO_PI * NextRandomFloat(seed);
    float x = r * cos(phi);
    float y = r * sin(phi);
    return float3(x, y, z);
}