#include "Common.h"

struct VoxelModel
{
	glm::mat4 Transform;
	std::vector<D3D12_RAYTRACING_AABB> AABBs;
};

class AABBBuilder
{
public:
	AABBBuilder();
	~AABBBuilder();

	void LoadAsAABBs(const std::string& voxFile);

private:
	std::shared_ptr<DXR::Device> mDevice;

	std::vector<VoxelModel> mModels;

	// Buffer for the Models
	ComPtr<DMA::Allocation> mVoxelModelBuffer;

};