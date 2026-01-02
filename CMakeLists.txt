cmake_minimum_required(VERSION 4.1)
project(WyrmEngine)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")

find_package(Vulkan REQUIRED)
find_package(KTX REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tinygltf REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 3.4 REQUIRED)

set(CMAKE_CXX_STANDARD 20)

add_executable(WyrmEngine main.cpp)

target_link_libraries(WyrmEngine
    Vulkan::Vulkan
    KTX::ktx
    nlohmann_json::nlohmann_json
    tinygltf::tinygltf
    glm::glm
    glfw
)
