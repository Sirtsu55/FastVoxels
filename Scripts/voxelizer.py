import trimesh as tm


mesh : tm.Trimesh = tm.load("Scenes/rusty.glb", force="mesh")


voxels : tm.voxel.VoxelGrid = tm.voxel.creation.voxelize(mesh, 1)


voxels.show()