import math
import sys
import numpy as np
import os
from pyvox.parser import VoxParser

file = sys.argv[1]
model_export_name = sys.argv[2]

if os.path.exists(file):
    scene = VoxParser(file).parse()
else:
    print("File not found")
    exit(0)

for i in range(0, len(scene.models)):
    model = scene.to_dense(i)

    save_file = f"Scenes/{model_export_name}_{i}.npy"
    
    np.save(save_file, model)

    print(f"Model resolution: {model.shape}")
    print(f"Model {i} saved to {save_file}")
