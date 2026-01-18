import os
import subprocess

entry_points = [
    "-entry", "vertMain",
    "-entry", "fragMain"
]

arguments = [
    "slangc", "shader.slang",
    "-target", "spirv",
    "-profile", "spirv_1_4",
    "-emit-spirv-directly",
    "-fvk-use-entrypoint-name",
]
arguments.extend(entry_points)
arguments.extend(["-o", "shader.spv"])

subprocess.run(arguments, check=True)

os.replace("shader.spv", "../../../cmake-build-debug/src/shaders/shader.spv")