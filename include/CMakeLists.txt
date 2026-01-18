target_include_directories(WyrmEngine PUBLIC .)

target_sources(WyrmEngine
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS .
    FILES
        WEngine.h
)
