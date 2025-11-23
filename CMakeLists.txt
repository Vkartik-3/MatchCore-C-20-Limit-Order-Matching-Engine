cmake_minimum_required(VERSION 3.20)
project(LimitOrderBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Main library
add_library(orderbook STATIC
    src/limit_order_book.cpp
)

target_include_directories(orderbook PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Platform-specific threading
find_package(Threads REQUIRED)
target_link_libraries(orderbook PRIVATE Threads::Threads)

# Demo executable
add_executable(orderbook_demo
    src/main.cpp
)

target_link_libraries(orderbook_demo PRIVATE orderbook)

# Tests executable
add_executable(orderbook_tests
    tests/orderbook_test.cpp
)

target_link_libraries(orderbook_tests PRIVATE orderbook)

# Installation rules
install(TARGETS orderbook orderbook_demo
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/ DESTINATION include)
