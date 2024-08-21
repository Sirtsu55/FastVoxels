import sys
import pandas as pd

file = sys.argv[1]

# Read the data from the file
data = pd.read_csv(file)
mean_frametime = data["FrameTime"].mean()
print(f"Mean Frame Time: {mean_frametime} ms")
print(f"Mean FPS: {((1/mean_frametime) * 1000)}")
