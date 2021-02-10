SET mypath=%~dp0

glslc %mypath%closesthit.rchit -o %mypath%closesthit.rchit.spv --target-env=vulkan1.2
glslc %mypath%miss.rmiss -o %mypath%miss.rmiss.spv --target-env=vulkan1.2
glslc %mypath%raygen.rgen -o %mypath%raygen.rgen.spv --target-env=vulkan1.2
glslc %mypath%shadow.rmiss -o %mypath%shadow.rmiss.spv --target-env=vulkan1.2
glslc %mypath%anyhit.rahit -o %mypath%anyhit.rahit.spv --target-env=vulkan1.2
glslc %mypath%shadow.rahit -o %mypath%shadow.rahit.spv --target-env=vulkan1.2
glslc %mypath%postprocess.vert -o %mypath%postprocess.vert.spv
glslc %mypath%postprocess.frag -o %mypath%postprocess.frag.spv
glslc %mypath%train_denoiser.comp -o %mypath%train_denoiser.comp.spv
