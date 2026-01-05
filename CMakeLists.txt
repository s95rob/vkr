cmake_minimum_required(VERSION 3.16)

project(vkr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# External dependencies

find_package(Vulkan REQUIRED)

# (Linux) Verify building GLFW against the correct windowing system
if (${LINUX})
    find_package(X11 QUIET)
    set(GLFW_BUILD_X11 OFF)
    set(GLFW_BUILD_WAYLAND OFF)

    if (X11_FOUND)
        set(GLFW_BUILD_X11 ON)
    endif()
endif()

add_subdirectory("extern/glfw")

# vkr

file(GLOB_RECURSE VKR_SRCS "src/*.cpp")
add_executable(vkr ${VKR_SRCS})
target_include_directories(vkr
PRIVATE
    "src"
    ${Vulkan_INCLUDE_DIRS}
    "extern/glm"
)
target_link_libraries(vkr
PRIVATE
    Vulkan::Vulkan
    Vulkan::Headers
    glfw
)
target_compile_definitions(vkr
PRIVATE
    $<$<BOOL:${LINUX}>:VKR_LINUX>
    $<$<BOOL:${WIN32}>:VKR_WIN32>
)

# Compile shaders
find_program(GLSLC_EXECUTABLE glslc)

file(GLOB_RECURSE VKR_SHADER_SRCS "src/*.vs.glsl" "src/*.fs.glsl")
set(VKR_SPIRV_BINARIES "")

foreach(SHADER_PATH ${VKR_SHADER_SRCS})
    get_filename_component(SHADER_NAME ${SHADER_PATH} NAME)
    string(REPLACE ".glsl" ".spv" SPIRV_NAME ${SHADER_NAME})
    set(SPIRV_OUT "${CMAKE_CURRENT_BINARY_DIR}/${SPIRV_NAME}")

    # Determine stage from extension
    if(SHADER_NAME MATCHES ".*\\.vs\\.glsl$")
        set(SHADER_STAGE "vertex")
    elseif(SHADER_NAME MATCHES ".*\\.fs\\.glsl$")
        set(SHADER_STAGE "fragment")
    endif()

    add_custom_command(
        OUTPUT ${SPIRV_OUT}
        COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=${SHADER_STAGE} ${SHADER_PATH} -o ${SPIRV_OUT}
        DEPENDS ${SHADER_PATH}
        COMMENT "Compiling ${SHADER_STAGE} shader: ${SHADER_BASE}.spv"
    )

    list(APPEND SPIRV_BINARIES ${SPIRV_OUT})
endforeach()

add_custom_target(vkr-shaders ALL DEPENDS ${SPIRV_BINARIES})
add_dependencies(vkr vkr-shaders)