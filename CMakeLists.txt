cmake_minimum_required(VERSION 3.20)

project(cpp_webserver
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json googletest)

add_executable(webserver
    src/main.cpp
    src/server.cpp
    src/server_config.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/router.cpp
    src/file_handler.cpp
    src/route_handler.cpp
    src/user_store.cpp
    src/logger.cpp
    src/socket_utils.cpp
    src/concurrency/thread_pool.cpp
)

target_link_libraries(webserver PRIVATE nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(webserver PRIVATE ws2_32)
endif()

enable_testing()

add_executable(webserver_tests
    tests/test_router.cpp
    tests/test_http_request.cpp
    tests/test_http_response.cpp
    tests/test_user_store.cpp
    tests/test_thread_pool.cpp
    tests/test_integration.cpp
    src/server.cpp
    src/server_config.cpp
    src/router.cpp
    src/http_request.cpp
    src/http_response.cpp
    src/user_store.cpp
    src/concurrency/thread_pool.cpp
    src/logger.cpp
    src/socket_utils.cpp
    src/file_handler.cpp
    src/route_handler.cpp
)

target_link_libraries(webserver_tests PRIVATE gtest_main nlohmann_json::nlohmann_json)

if(WIN32)
    target_link_libraries(webserver_tests PRIVATE ws2_32)
endif()

add_test(NAME webserver_tests COMMAND webserver_tests)

add_executable(benchmark_runner
    benchmark/benchmark_runner.cpp
)

if(WIN32)
    target_link_libraries(benchmark_runner PRIVATE ws2_32)
endif()