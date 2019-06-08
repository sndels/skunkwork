cmake_minimum_required(VERSION 3.12)

project(skunkwork)

# Set up external dependencies
if (NOT APPLE)
    find_package(OpenMP REQUIRED)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()
find_package(OpenGL REQUIRED)

# Set up sub-builds and sources
add_subdirectory(ext)
add_subdirectory(include)
add_subdirectory(src)

# Set absolute path to res directory
add_definitions(-DRES_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/res/")

# Set up project targets
# WIN32 tells to not build a cmd-app on windows
add_executable(skunkwork WIN32 ${SKUNKWORK_SOURCES})
target_compile_features(skunkwork
    PRIVATE
    cxx_std_17
)
if (MSVC)
    target_compile_options(skunkwork
        PRIVATE
        /Wall
    )
else()
    target_compile_options(skunkwork
        PRIVATE
        -Wall
        -pedantic
        -Xpreprocessor -fopenmp -lomp
    )
endif()
target_compile_definitions(skunkwork PRIVATE ROCKET)
target_include_directories(skunkwork
    PRIVATE
    ${SKUNKWORK_INCLUDE_DIR}
)
target_link_libraries(skunkwork
    PRIVATE
    ${OPENGL_LIBRARIES}
    bass
    glfw
    glm
    libgl3w
    librocket
    imgui
)

# Copy dynamic libraries to build folder after build
# A cleaner way of doing this even with msvc's build structure?
add_custom_command(TARGET skunkwork POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                           ${CMAKE_SOURCE_DIR}/ext/bass/lib
                           $<TARGET_FILE_DIR:skunkwork>)

# "shadertoy" only, doesn't include audio or rocket support
add_executable(skunktoy WIN32 ${SKUNKTOY_SOURCES})
target_compile_features(skunktoy
    PRIVATE
    cxx_std_17
)
if (MSVC)
    target_compile_options(skunktoy
        PRIVATE
        /Wall
    )
else()
    target_compile_options(skunktoy
        PRIVATE
        -Wall
        -pedantic
    )
endif()
target_include_directories(skunktoy
    PRIVATE
    ${SKUNKWORK_INCLUDE_DIR}
)
if (APPLE)
    target_include_directories(skunktoy
        PRIVATE
        /usr/local/Cellar/libomp/8.0.0/include
    )
endif()
target_link_libraries(skunktoy
    PRIVATE
    ${OPENGL_LIBRARIES}
    glfw
    glm
    libgl3w
    imgui 
)
