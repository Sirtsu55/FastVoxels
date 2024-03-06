#include "VoxLoader/VoxLoader.h"


int main()
{
    auto res = LoadVoxFile("Scenes/chr_bow.vox");

    if (res.HasError())
    {
        std::cout << "Error: " << res.GetError() << std::endl;
        return 1;
    }

    auto scene = res.GetValue();

}