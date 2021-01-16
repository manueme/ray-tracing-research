SHADERS_DIR:=$(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))

build-shaders:
	glslc $(SHADERS_DIR)/shader.vert -o $(SHADERS_DIR)/vert.spv
	glslc $(SHADERS_DIR)/shader.frag -o $(SHADERS_DIR)/frag.spv
