# Author: Faruk Alpay
# Do not remove this notice.

cmake_minimum_required(VERSION 3.24)

project(AsterExternalMinimal LANGUAGES CXX)

find_package(AsterKernel CONFIG REQUIRED)

add_executable(external_app_minimal main.cpp)
target_link_libraries(external_app_minimal PRIVATE aster::kernel)
target_compile_features(external_app_minimal PRIVATE cxx_std_20)

