# Ray Tracing Research

## Introduction

In this project, it is proposed to know more about the possibilities of RT cores for ray tracing development, as well as
their real advantages compared to CPU or GPU development of previous generations. It is also proposed to study the
Artificial Intelligence techniques developed by NVIDIA, and other research groups, for the improvement, denoising, and
acceleration of the generation of images with ray tracing. For this, one or more ray-tracing applications will be
carried out, which may have different objectives: physically realistic ray tracing, real-time ray tracing (with AI-based
denoising), path tracing and other methodologies suitable for daylighting analysis.

## Implemented Applications

* Monte carlo ray tracing
* Monte carlo ray tracing with Optix denoiser
* Ray tracing with raster pipeline first hit (hybrid pipeline)

## Screenshots

![Sponza 1](/screenshots/sponza_1.png?raw=true)
![Sponza 2](/screenshots/sponza_2.png?raw=true)
![Sponza 3](/screenshots/sponza_3.png?raw=true)
![Conrell Box 1](/screenshots/cornell_box_1.png?raw=true)
![Pool 1](/screenshots/pool_1.png?raw=true)
![Pool 2](/screenshots/pool_2.png?raw=true)
![Pool 3](/screenshots/pool_3.png?raw=true)
![Pool 4](/screenshots/pool_4.png?raw=true)

## Contact

Manuel Machado: [Github](https://github.com/manueme) / [Twitter](https://twitter.com/Manueme7)
/ [LinkedIn](https://www.linkedin.com/in/manueme/)

## Status <a href="https://www.repostatus.org/#wip"><img src="https://www.repostatus.org/badges/latest/wip.svg" alt="Project Status: WIP – Initial development is in progress, but there has not yet been a stable, usable release suitable for the public." /></a>

This project is still a work in progress.

## External Sources

### Vulkan SDK

Download the LunarG SDK from: https://vulkan.lunarg.com/sdk/home

### GLFW

#### Windows

Run `build_glfw_windows.bat`

The script will clone (https://github.com/glfw/glfw) and build GLFW. Don't forget to check recent issues on Windows
building if the process fails, you can always checkout to the last stable tag release, for example
[v3.3.2](https://github.com/glfw/glfw/commit/0a49ef0a00baa3ab520ddc452f0e3b1e099c5589)

CMake will take care of the rest, if you change the generator make sure to also update the paths in the GLFW section of
the CMakeLists.txt to target the new libraries

#### Linux

`sudo apt install libglfw3-dev libglfw3`

### GLM

GLM is imported within a git submodule (external_sources/glm), don't forget to initialize it.

### FreeImage

#### Windows

1- Download the **Binary distribution** from http://freeimage.sourceforge.net/.

2- Copy the **Dist** folder to `external_sources` and rename it to `freeImage`, the `x32` and `x64` folders with
the `FreeImage.dll`, `FreeImage.h` and `FreeImage.lib` files must be directly under `freeImage` _(
/external_sources/freeImage/...)_

Note: for this project v3.18.0 was used

TODO: Make a script to take care of this process

#### Linux

Install FreeImage from repository package:

`sudo apt-get install libfreeimage-dev`

### Assimp

The following scripts will clone (https://github.com/assimp/assimp) and build Assimp. Don't forget to check recent issues on
Windows/Linux building if the process fails, you can always checkout to the last stable tag release, for example
[v5.0.1](https://github.com/assimp/assimp/commit/8f0c6b04b2257a520aaab38421b2e090204b69df)

CMake will take care of the rest, if you change the generator make sure to also update the paths in the Assimp section
of the CMakeLists.txt to target the new libraries

#### Windows

Run `build_assimp_windows.bat`

#### Linux

Run `build_assimp_linux.sh`

### CUDA (only required for to run the denoiser app)

Install the CUDA SDK from [Cuda Zone](https://developer.nvidia.com/cuda-zone)

### Optix (only required for to run the denoiser app)

Install **Optix 3.0** from the [NVIDIA Developer page](https://developer.nvidia.com/designworks/optix/download), you will
probably need an NVIDIA developer account for this.

#### Linux

By default the **Find OptiX** will look for `OPTIX_PATH` as an environment variable with the installation path or
into _"~/NVIDIA-OptiX-SDK-3.0.0-linux64"_. You can change the path defined in `cmake/FindOptiX.cmake`.

#### Windows

By default the **Find OptiX** will look for `OPTIX_PATH` as an environment variable with the installation path or
into "_C:/ProgramData/NVIDIA Corporation/OptiX SDK 3.0.0"_, Optix should be installed there by default, if not, you can
change the path defined in `cmake/FindOptiX.cmake`.

## Assets

Download the assets folder from https://drive.google.com/open?id=1-JLF8sU_Lb6ZqrRwocwZP3AaiztOekPO
and extract it on the root directory of the project ```/assets/...```

_Note: The zip has a password (for now) in order to protect some models copyrights._

_TODO: create a free public asset folder to download_

## Code style and naming conventions

There's a `.clang-format` and `.clang-tidy` file defined for the project, in order to write code install and configure
on your IDE, **Clangd:** https://clangd.llvm.org/installation.html, or both **Clang-Format** and **Clang-Tidy**.

*Note: you may also want to remove any naming convention settings on your IDE to prevent overriding the clang-tidy
readability checks*

## Acknowledgements

* My first steps in Vulkan were possible thanks to [Vulkan Tutorial](https://vulkan-tutorial.com/).

* The first steps of this project were carried out following the demos and examples referenced on
  the [official Khronos website](https://www.khronos.org/vulkan):
    * [Vulkan reference material / framework](https://github.com/KhronosGroup/Vulkan-Samples) by Khronos.
    * [Sascha Willems' examples and demos](https://github.com/SaschaWillems/Vulkan), IMO the best samples and demos out
      there to get started in Vulkan.
    * [NVIDIA DeignWorks Samples](https://github.com/nvpro-samples).
