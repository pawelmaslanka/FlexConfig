cmake_minimum_required(VERSION 3.26.3)

project(FlexConfig LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_DIR src)

include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/node
    ${SRC_DIR}/lib
)

set(CPP_HEADER_EXT hpp)
set(CPP_SRC_EXT cpp)

set(INC_LIST
    "${SRC_DIR}/lib/topo_sort.${CPP_HEADER_EXT}"
    "${SRC_DIR}/load_config.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/node.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/composite.${CPP_HEADER_EXT}"
    "${SRC_DIR}/node/visitor.${CPP_HEADER_EXT}"
)

set(SRC_LIST
    "${SRC_DIR}/expr_eval.${CPP_SRC_EXT}"
    "${SRC_DIR}/lib/topo_sort.${CPP_SRC_EXT}"
    "${SRC_DIR}/load_config.${CPP_SRC_EXT}"
    "${SRC_DIR}/node/node.${CPP_SRC_EXT}"
    "${SRC_DIR}/node/composite.${CPP_SRC_EXT}"
    "${SRC_DIR}/value.${CPP_SRC_EXT}"
    "${SRC_DIR}/xpath.${CPP_SRC_EXT}"
)

add_executable(${PROJECT_NAME} ${INC_LIST} ${SRC_LIST} main.${CPP_SRC_EXT})
