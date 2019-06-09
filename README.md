# cubnature
![screenshot](screenshot.png)

## Dependencies
Building requires OpenGL dev libraries. [BASS](http://www.un4seen.com/bass.html) is included as a dynamic library under its non-commercial license, while [GLFW3](http://www.glfw.org), [dear imgui](https://github.com/ocornut/imgui), [Rocket](https://github.com/rocket/rocket) and [gl3w](https://github.com/sndels/libgl3w) are submodules with their respective licenses.

## Building 
cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j9 && ./cubnature
