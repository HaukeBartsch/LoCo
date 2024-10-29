cmake_minimum_required (VERSION 3.16.0)

project (renderStody)

#set (CMAKE_CXX_STANDARD 17)
IF("${CMAKE_SYSTEM}" MATCHES "Linux")
   set (CMAKE_CXX_STANDARD 17)
   SET(CMAKE_CXX_FLAGS "-std=c++2a -I/usr/local/include -I/usr/include")
ELSE()
   # find the correct path for isysroot with `xcrun --show-sdk-path`
   # /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk
   # SET(CMAKE_CXX_FLAGS "-std=c++7 -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk -I/usr/local/include")
   set (CMAKE_CXX_STANDARD 20)
   SET(CMAKE_CXX_FLAGS "-std=c++20 -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk -I/usr/local/include")
ENDIF()

FIND_LIBRARY(JPEG_LIBRARY NAMES jpeg)
FIND_LIBRARY(ZLIB_LIBRARY NAMES z)
FIND_LIBRARY(XLST_LIBRARY NAMES exslt)
FIND_LIBRARY(PNG_LIBRARY NAMES png16)

message(STATUS JPEG_LIBRARY = ${JPEG_LIBRARY})
message(STATUS ZLIB_LIBRARY = ${ZLIB_LIBRARY})
message(STATUS XLST_LIBRARY = ${XLST_LIBRARY})
message(STATUS PNG_LIBRARY = ${PNG_LIBRARY})

# Boost
set(boost_min_ver 1.70.0)
set(boost_libs system filesystem timer date_time program_options)
find_package(Boost ${boost_min_ver})

if(Boost_FOUND)
    find_package(Boost ${boost_min_ver} COMPONENTS ${boost_libs})
endif()

# Freetype
find_package(Freetype REQUIRED)

add_executable (renderStory main.cpp ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (renderStory PUBLIC ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(renderStory ${JPEG_LIBRARY} ${ZLIB_LIBRARY} ${XLST_LIBRARY} ${Boost_LIBRARIES} ${FREETYPE_LIBRARIES} ${PNG_LIBRARY} pthread)
