cmake_minimum_required(VERSION 3.14)
project(GoQuantSim LANGUAGES CXX)

# 1) Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2) Use vcpkg’s toolchain if provided
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

# 3) Find all necessary packages
find_package(ixwebsocket    CONFIG REQUIRED)
find_package(nlohmann_json  CONFIG REQUIRED)
find_package(spdlog         CONFIG REQUIRED)
find_package(OpenSSL        REQUIRED)
find_package(Catch2         CONFIG REQUIRED)

# 4) Core library from OrderBook
add_library(simulator_lib
    OrderBook.cpp
)
target_include_directories(simulator_lib PUBLIC ${CMAKE_SOURCE_DIR})
target_link_libraries(simulator_lib
    PUBLIC
      ixwebsocket::ixwebsocket
      nlohmann_json::nlohmann_json
      spdlog::spdlog
      OpenSSL::SSL
      OpenSSL::Crypto
)

# 5) Main simulator executable
add_executable(simulator
    main.cpp
)
target_link_libraries(simulator
    PRIVATE
      simulator_lib
      bcrypt
      Ws2_32
)

enable_testing()
# 6) Unit tests
add_subdirectory(tests)
