target_include_directories(WyrmEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_sources(WyrmEngine
PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        WEngine.h
)