target_include_directories(WyrmRenderer PRIVATE .)

target_sources(WyrmRenderer
PRIVATE
    vk_mem_alloc.h
    WRenderer.cpp
)