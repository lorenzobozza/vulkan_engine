![Static Badge](https://img.shields.io/badge/Acinonyx-0.3_alpha-blue?labelColor=323C3C&) ![Static Badge](https://img.shields.io/badge/Vulkan-1.4.341-2DBE50?labelColor=323C3C&logo=vulkan&logoColor=red) ![Static Badge](https://img.shields.io/badge/SDL-3.4.0-blue?labelColor=323C3C) ![Static Badge](https://img.shields.io/badge/Bullet-2.25-orange?labelColor=323C3C) ![Static Badge](https://img.shields.io/badge/glTF-2DBE50?labelColor=323C3C&logo=gltf)

# Project Acinonyx

Acinonyx is a cross platform 3D engine based on C++23 and Vulkan, it features Forward Rendering, Physically Based Shading, Shadows, Irradiance Volume GI, Physics, glTF support and more.
This project is actively being developed by me on spare time as an electronics engineering student to explore modern real-time 3D technologies.

Some future improvements will be Forward+ Rendering, Hybrid Rendering, Ray Traced GI and AO, Animations and much more. And obviously as the name implies, the main goal is to be stupidly fast and over-optimized.

![preview](assets/preview/sponza_example.png "Sponza")
![preview](assets/preview/physics_example.gif "Physics")

## Build instructions
### Prerequisites
#### Premake 5
Install through your package manager or download from https://github.com/premake/premake-core/releases
#### Vulkan SDK (includes SDL3)
Available at https://vulkan.lunarg.com/sdk/home

After setting up the dependencies go to the repository root folder and run the premake script to generate the project files for your platform of choice
```bash
# macOS
premake5 xcode4

# Windows
premake5 vs2026

# Linux (ninja requires at least premake5 alpha 8 version)
premake5 ninja
```