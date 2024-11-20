
file(GLOB_RECURSE SHARE_SRC_FILES
    "${CMAKE_SOURCE_DIR}/src/collections/*.c"
    "${CMAKE_SOURCE_DIR}/src/io/*.c"
    "${CMAKE_SOURCE_DIR}/src/entity/*.c"
)

message(STATUS "CMAKE_SOURCE_DIR is: ${CMAKE_SOURCE_DIR}")
message(STATUS "share src files: ${SHARE_SRC_FILES}")
message(STATUS "The value of CMAKE_VERBOSE_MAKEFILE is : ${CMAKE_VERBOSE_MAKEFILE}")

add_executable(IMServer IMServer.c net_util.c ${SHARE_SRC_FILES})
add_executable(IMClient IMClient.c net_util.c ${SHARE_SRC_FILES})

target_include_directories(IMServer PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(IMClient PRIVATE ${CMAKE_SOURCE_DIR}/include)