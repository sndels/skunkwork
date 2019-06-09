cubnature

music - hyrtsi
music - keksi
code - lehdari
code, graphics, direction - sndels
code, graphics - tkln

(skunkwork) Dependencies
Building requires OpenGL dev libraries and glfw-dependencies. BASS is included as
a dynamic library under its non-commercial license, while GLFW3, imgui, Rocket and
gl3w are submodules with their respective licenses.

Building 
cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make -j9 && cp ../sync/*.track . && ./cubnature
