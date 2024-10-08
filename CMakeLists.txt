cmake_minimum_required(VERSION 3.20.1)

project(LOCO)

# find the correct path for isysroot with `xcrun --show-sdk-path`
set (CMAKE_CXX_STANDARD 20)

# Boost
set(boost_min_ver 1.50.0)
set(boost_libs system filesystem timer date_time program_options)
find_package(Boost ${boost_min_ver})

if(Boost_FOUND)
  find_package(Boost ${boost_min_ver} COMPONENTS ${boost_libs})
endif()

find_package(TBB REQUIRED)

if(TBB_FOUND)
  MESSAGE(STATUS ">>> Found TBB")
ENDIF()

find_package(LIBZIP REQUIRED)
if (LIBZIP_FOUND)
   MESSAGE(STATUS ">>> Found LIBZIP")
ELSE(LIBZIP_FOUND)
  MESSAGE(FATAL_ERROR "Cannot build app without libzip.")
ENDIF()


# target_link_libraries(your_target ${Boost_LIBRARIES})
add_executable(LoCo LoCo.cpp history.hpp)
target_include_directories(LoCo PUBLIC)
target_link_libraries(LoCo ${Boost_LIBRARIES} TBB::tbb)

