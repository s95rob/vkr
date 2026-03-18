# vkr

<p align="center">
<img width="602" height="519" alt="image" src="https://github.com/user-attachments/assets/0e6d0ba6-bc89-497b-bb9d-c6665d92f60d" />
</p>

Work in progress Vulkan renderer from scratch. Aiming to support a complete PBR lighting model using an extensible design for render pass configuration. 

## TODO:
- [ ] Depth texture support (In progress)
- [ ] PBR material system (Planned)
- [ ] Dynamic render pass graph (Planned)

## Getting Started

### Requirements
+ C++20 compiler:
  - MSVC: Visual Studio 2019 (version 16.11+)
  - GCC: 11.0+
  - Clang: 15.0+
+ [CMake 3.16+](https://cmake.org/)
+ [Vulkan SDK 1.3+](https://vulkan.lunarg.com/sdk/home)

### Building
1. Clone this repository recursively.
2. Configure with CMake.
3. Run the generated Makefile.


*NOTE: Only tested on Linux using Clang 19.1.7*
