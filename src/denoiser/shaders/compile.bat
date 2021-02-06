SET mypath=%~dp0

SET monte_carlo_app_shaders_path=%mypath%..\..\monte_carlo_ray_tracing\shaders\

glslc %monte_carlo_app_shaders_path%closesthit.rchit -o %mypath%closesthit.rchit.spv --target-env=vulkan1.2
glslc %monte_carlo_app_shaders_path%miss.rmiss -o %mypath%miss.rmiss.spv --target-env=vulkan1.2
glslc %monte_carlo_app_shaders_path%raygen.rgen -o %mypath%raygen.rgen.spv --target-env=vulkan1.2
glslc %monte_carlo_app_shaders_path%shadow.rmiss -o %mypath%shadow.rmiss.spv --target-env=vulkan1.2
glslc %monte_carlo_app_shaders_path%anyhit.rahit -o %mypath%anyhit.rahit.spv --target-env=vulkan1.2
glslc %monte_carlo_app_shaders_path%shadow.rahit -o %mypath%shadow.rahit.spv --target-env=vulkan1.2
glslc %monte_carlo_app_shaders_path%postprocess.vert -o %mypath%postprocess.vert.spv
glslc %monte_carlo_app_shaders_path%postprocess.frag -o %mypath%postprocess.frag.spv
