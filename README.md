## Building 
### Prerequisites
- [Download and install Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
- [Download and install Vulkan SDK](https://vulkan.lunarg.com/)
- [Download and install CMake](https://cmake.org/download/)
  - Make sure it's added to the system path.
- [Download GLFW](https://www.glfw.org/download.html) (64-bit precompiled binary)
- [Download GLM](https://github.com/g-truc/glm/releases)
- Rename "envWindowsExample.cmake" to "windows.env.cmake"
- Update the filepath variable to your install locations

- In windows powershell
```
cd Wolfie3D
cmake -S . -B ./build/
```
- Open the solution file, Right-lick Wolfie3D and set as startup project.
- Build and Run