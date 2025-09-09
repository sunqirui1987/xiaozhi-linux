cmake_minimum_required(VERSION 3.22)
project(linx)


set(CILL_INC ${CMAKE_CURRENT_LIST_DIR})
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBWEBSOCKETS REQUIRED libwebsockets)

file(GLOB_RECURSE CILL_CXX_SRC ${CMAKE_CURRENT_LIST_DIR}/*.cc)
file(GLOB_RECURSE CILL_C_SRC ${CMAKE_CURRENT_LIST_DIR}/*.c)
list(APPEND CPP_FILES ${CILL_CXX_SRC} ${CILL_C_SRC})

add_library(${PROJECT_NAME} STATIC 
    ${CPP_FILES}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CILL_INC}/thirdparty/spdlog/include
    ${CILL_INC}/thirdparty/nlohmann_json/include
    ${CILL_INC}/websocket/include
    ${CILL_INC}/alsaaudio/include
    ${CILL_INC}/audio/include
    ${CILL_INC}/opus/include
    ${CILL_INC}/filestream/include
    ${CILL_INC}/http/include
    ${CILL_INC}/json/include
    ${CILL_INC}/log/include
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)

# Platform-specific libraries
if(APPLE)
    target_link_directories(${PROJECT_NAME} PUBLIC
        /opt/homebrew/opt/portaudio/lib
        /opt/homebrew/opt/opus/lib
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${LIBWEBSOCKETS_LINK_LIBRARIES}
        m
        dl
        opus
        portaudio
        Threads::Threads
        ssl
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework AudioUnit"
    )
else()
    target_link_libraries(${PROJECT_NAME} PUBLIC
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${LIBWEBSOCKETS_LINK_LIBRARIES}
        m
        rt
        dl
        opus
        asound
        Threads::Threads
        ssl
    )
endif()
