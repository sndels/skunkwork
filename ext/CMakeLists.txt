# Pull submodules
execute_process(COMMAND git submodule update --init --depth 1)

# GLM is header only
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE glm)

# Libraries
add_subdirectory(libgl3w)

# Override glfw default options
set(SDL_SHARED OFF CACHE BOOL "Build a shared version of the library")
set(SDL_STATIC ON CACHE BOOL "Build a static version of the library")
set(SDL_TEST OFF CACHE BOOL "Build the SDL3_test library")
add_subdirectory(SDL)

add_library(imgui STATIC "")
target_sources(imgui
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/examples/imgui_impl_sdl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/imgui/examples/imgui_impl_opengl3.cpp
)
target_include_directories(imgui
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/imgui
    ${CMAKE_CURRENT_LIST_DIR}/imgui/examples
)
target_link_libraries(imgui
    PUBLIC
    ${OPENGL_LIBRARIES}
    libgl3w
    SDL2-static
)

add_library(librocket STATIC "")
target_sources(librocket
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/rocket/lib/device.c
    ${CMAKE_CURRENT_LIST_DIR}/rocket/lib/track.c
)
target_include_directories(librocket
    PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/rocket/lib
)

if(MSVC)
    target_link_libraries(librocket
        PRIVATE
        ws2_32.lib
    )
endif()

add_library(bass INTERFACE)
target_include_directories(bass
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/bass
)

if(MSVC)
    target_link_libraries(bass
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/bass/lib/bass.lib
    )
elseif(APPLE)
    target_link_libraries(bass
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/bass/lib/libbass.dylib
    )
else()
    target_link_libraries(bass
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/bass/lib/libbass.so
    )
endif()
