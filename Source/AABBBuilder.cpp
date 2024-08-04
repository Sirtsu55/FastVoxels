#include "AABBBuilder.h"
#include "FileRead.h"

#include "ogt_vox.h"


AABBBuilder::AABBBuilder()
{
}

AABBBuilder::~AABBBuilder()
{
}

void AABBBuilder::LoadAsAABBs(const std::string& voxFile)
{
	std::vector<uint8_t> rawVox;
	FileRead("Assets/CountrySide_Source.vox", rawVox);
	auto voxScene = ogt_vox_read_scene(rawVox.data(), rawVox.size());
	rawVox.clear();

	for (uint32_t i = 0; i < voxScene->num_instances; i++)
	{
		auto& instance = voxScene->instances[i];
		auto& model = voxScene->models[instance.model_index];

		VoxelModel& voxelModel = mModels.emplace_back();

		voxelModel.Transform = glm::transpose(glm::make_mat4(&instance.transform.m00));

		uint32_t sizeX = model->size_x;
		uint32_t sizeY = model->size_y;
		uint32_t sizeZ = model->size_z;

		// Iterate over the voxels in the model
		for (uint32_t x = 0; x < sizeX; x++)
		{
			for (uint32_t y = 0; y < sizeY; y++)
			{
				for (uint32_t z = 0; z < sizeZ; z++)
				{
					if (model->voxel_data[x + y * sizeX + z * sizeX * sizeY] != 0)
					{
					}
				}
			}
		}
	}

}
