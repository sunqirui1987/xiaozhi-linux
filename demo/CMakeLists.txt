cmake_minimum_required(VERSION 3.22)
project(linx_demo)


file(GLOB_RECURSE TEST_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cc)
list(APPEND CPP_FILES ${TEST_SRC})

add_executable(linx_app ${CPP_FILES})

target_link_libraries(linx_app PRIVATE
    linx
)

install(TARGETS linx_app RUNTIME DESTINATION bin)