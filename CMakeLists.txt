cmake_minimum_required(VERSION 3.13)
project("roa-game-data-hook" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

file(READ version ROA_GAME_DATA_HOOK_VERSION)

file(GLOB_RECURSE CXX_SOURCE "src/*.cpp")

add_library(${PROJECT_NAME} SHARED ${CXX_SOURCE})
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "${PROJECT_NAME}")
target_include_directories(${PROJECT_NAME} PRIVATE "src")

# vendors
# roa-mod-loader
add_subdirectory("vendor/roa-mod-loader")
target_link_libraries(${PROJECT_NAME} PRIVATE roa-mod-loader)
target_include_directories(${PROJECT_NAME} PRIVATE "vendor/roa-mod-loader/loader/include")


target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp::yaml-cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/details.yaml" "${CMAKE_BINARY_DIR}/Release/${PROJECT_NAME}/info.yaml"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/config.yaml" "${CMAKE_BINARY_DIR}/Release/${PROJECT_NAME}/config.yaml"
)