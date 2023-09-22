## Building 
### Prerequisites
- [Download and install Visual Studio 2022](https://visualstudio.microsoft.com/vs/)
- [Download and install Vulkan SDK](https://vulkan.lunarg.com/)
- [Download and install CMake](https://cmake.org/download/)
  - Make sure it's added to the system path.



### Build Steps
- Clone the repo
- switch to the HW branch
- ```git submodule update --init --recursive``` to init the submodules

- In windows powershell
```
cd Wolfie3D
cmake -S . -B ./build/
```
- Build and Run 
