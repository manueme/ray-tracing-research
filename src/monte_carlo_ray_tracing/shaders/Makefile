SHADERS_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

build-shaders:
	glslc $(SHADERS_DIR)/closesthit.rchit -o $(SHADERS_DIR)/closesthit.rchit.spv --target-env=vulkan1.2
	glslc $(SHADERS_DIR)/miss.rmiss -o $(SHADERS_DIR)/miss.rmiss.spv --target-env=vulkan1.2
	glslc $(SHADERS_DIR)/raygen.rgen -o $(SHADERS_DIR)/raygen.rgen.spv --target-env=vulkan1.2
	glslc $(SHADERS_DIR)/shadow.rmiss -o $(SHADERS_DIR)/shadow.rmiss.spv --target-env=vulkan1.2
	glslc $(SHADERS_DIR)/anyhit.rahit -o $(SHADERS_DIR)/anyhit.rahit.spv --target-env=vulkan1.2
	glslc $(SHADERS_DIR)/shadow.rahit -o $(SHADERS_DIR)/shadow.rahit.spv --target-env=vulkan1.2
	glslc $(SHADERS_DIR)/final.vert -o $(SHADERS_DIR)/final.vert.spv
	glslc $(SHADERS_DIR)/final.frag -o $(SHADERS_DIR)/final.frag.spv
