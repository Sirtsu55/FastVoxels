#include "npy.hpp"

int main()
{
    auto model = npy::read_npy<uint8_t>("Scenes/model.npy");

    for (auto i : model.shape)
    {
        std::cout << i << "\n";
    }



}