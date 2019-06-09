cmake_minimum_required(VERSION 3.9)

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
    )
endif()
if (APPLE)
    target_include_directories(skunkwork
        PRIVATE
        /usr/local/Cellar/libomp/8.0.0/include
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
if (APPLE)
    target_link_libraries(skunkwork
        PRIVATE
        /usr/local/Cellar/libomp/8.0.0/lib/libomp.a
    )
endif()

# Copy dynamic libraries to build folder after build
# A cleaner way of doing this even with msvc's build structure?
add_custom_command(TARGET skunkwork POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                           ${CMAKE_SOURCE_DIR}/ext/bass/lib
                           $<TARGET_FILE_DIR:skunkwork>)
