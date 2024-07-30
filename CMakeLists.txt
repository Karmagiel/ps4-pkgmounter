cmake_minimum_required(VERSION 3.27)
project(ps4_pkgmount)

set(CMAKE_CXX_STANDARD 20)

# add_executable(ps4compat main.cpp pkg.cpp pkg_ids.cpp)
#add_compile_options(fuse_test )
#add_executable(fuse_test fuse_test.cpp)
add_executable(ps4_pkgmount pkgmount.cpp pkg.cpp pkg_ids.cpp)
target_include_directories(ps4_pkgmount PRIVATE /usr/include/fuse3)
target_link_libraries(ps4_pkgmount fuse3 pthread)