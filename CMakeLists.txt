cmake_minimum_required(VERSION 3.0)

project(pbd VERSION 0.0.1)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/thirdparty/proto-cmake/cmake)

find_package(Protobuf REQUIRED)

FILE(GLOB PROTOS examples/protos/*.proto)

set(PROTO_SRC_DIR ${PROJECT_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})

add_executable(example
    examples/src/example.cpp
    ${PROTO_SRCS}
)
target_include_directories(example PUBLIC ${PROTO_SRC_DIR} cpp/src/include)
target_link_libraries(example
    protobuf::libprotobuf
)
