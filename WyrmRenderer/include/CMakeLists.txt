target_include_directories(WyrmRenderer PUBLIC .)

target_sources(WyrmRenderer
PUBLIC
    FILE_SET HEADERS
    BASE_DIRS .
    FILES
        WRenderer.h
)