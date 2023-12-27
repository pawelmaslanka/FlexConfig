cmake_minimum_required(VERSION 3.26.3)

project(FlexConfig LANGUAGES CXX)

find_package(fmt 9.1.0 REQUIRED)
# Seeking lib as seen in /usr/local/lib/cmake/nlohmann_json_schema_validator
find_package(nlohmann_json_schema_validator 2.2.0 REQUIRED)
find_package(nlohmann_json 3.11.2 REQUIRED)
find_package(spdlog 1.11.0 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR src)

include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/node
    ${SRC_DIR}/lib
    ${SRC_DIR}/lib/external/args
    ${SRC_DIR}/lib/external/httplib
    ${SRC_DIR}/lib/external/nlohmann
    ${SRC_DIR}/lib/external/peglib
)

set(CPP_HEADER_EXT hpp)
set(CPP_SRC_EXT cpp)

set(INC_LIST
    "${SRC_DIR}/lib/topo_sort.${CPP_HEADER_EXT}"
    "${SRC_DIR}/config.${CPP_HEADER_EXT}"
    "${SRC_DIR}/connection_management.${CPP_HEADER_EXT}"
    "${SRC_DIR}/constraint_checking.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/node.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/leaf.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/composite.${CPP_HEADER_EXT}"
    "${SRC_DIR}/session_management.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/visitor.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/visitor_spec.${CPP_HEADER_EXT}"
    "${SRC_DIR}/nodes_ordering.${CPP_HEADER_EXT}"
)

set(SRC_LIST
    "${SRC_DIR}/lib/topo_sort.${CPP_SRC_EXT}"
    "${SRC_DIR}/config.${CPP_SRC_EXT}"
    "${SRC_DIR}/connection_management.${CPP_SRC_EXT}"
    "${SRC_DIR}/constraint_checking.${CPP_SRC_EXT}"
    "${SRC_DIR}/node/node.${CPP_SRC_EXT}"
    "${SRC_DIR}/node/leaf.${CPP_SRC_EXT}"
    "${SRC_DIR}/node/composite.${CPP_SRC_EXT}"
    "${SRC_DIR}/nodes_ordering.${CPP_SRC_EXT}"
    "${SRC_DIR}/session_management.${CPP_SRC_EXT}"
    "${SRC_DIR}/value.${CPP_SRC_EXT}"
    "${SRC_DIR}/xpath.${CPP_SRC_EXT}"
)

add_executable(${PROJECT_NAME} ${INC_LIST} ${SRC_LIST} main.${CPP_SRC_EXT})
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json spdlog::spdlog_header_only nlohmann_json_schema_validator::validator)
