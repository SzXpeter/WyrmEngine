cmake_minimum_required(VERSION 4.1)
project(WyrmEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/CMake")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overriding-option")

find_package(Vulkan REQUIRED)
find_package(KTX REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tinygltf REQUIRED)
find_package(glm REQUIRED)
find_package(glfw3 3.4 REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(include)
add_subdirectory(src)

target_link_libraries(${PROJECT_NAME}
    Vulkan::Vulkan
    KTX::ktx
    nlohmann_json::nlohmann_json
    tinygltf::tinygltf
    glm::glm
    glfw
)
