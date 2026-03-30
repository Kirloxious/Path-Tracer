cmake_minimum_required(VERSION 3.21)
project(main LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra)

# -----------------------------
# GLAD (compile glad.c)
# -----------------------------
add_library(glad external/glad/glad.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/external)

# -----------------------------
# Main executable
# -----------------------------
file(GLOB_RECURSE SRC_FILES src/*.cpp)

add_executable(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# -----------------------------
# OpenGL
# -----------------------------
find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} OpenGL::GL)

# -----------------------------
# GLFW
# -----------------------------
include(FetchContent)
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    message(STATUS "GLFW not found, fetching...")
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
    )
    FetchContent_MakeAvailable(glfw)
endif()
target_link_libraries(${PROJECT_NAME} glfw)

# -----------------------------
# GLM
# -----------------------------
find_package(glm QUIET)
if(NOT glm_FOUND)
    message(STATUS "GLM not found, fetching...")
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
    )
    FetchContent_MakeAvailable(glm)
endif()
target_link_libraries(${PROJECT_NAME} glm::glm)


target_sources(${PROJECT_NAME} PRIVATE
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE external/imgui external/imgui/backends)






# -----------------------------
# Link GLAD last
# -----------------------------
target_link_libraries(${PROJECT_NAME} glad)

target_compile_definitions(main PRIVATE _ENABLE_EXTENDED_ALIGNED_STORAGE)