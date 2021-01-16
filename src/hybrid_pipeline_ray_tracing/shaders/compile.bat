SET mypath=%~dp0

glslc %mypath%shader.vert -o %mypath%vert.spv
glslc %mypath%shader.frag -o %mypath%frag.spv