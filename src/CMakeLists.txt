target_include_directories(WyrmEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(WyrmEngine
PRIVATE
    Structs.h
    WEngine.cpp
    VulkanInit.cpp
)