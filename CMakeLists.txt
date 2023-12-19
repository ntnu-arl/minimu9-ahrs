cmake_minimum_required(VERSION 3.10)

project(minimu9)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use the Eigen library.
find_package(Eigen3 REQUIRED)
include_directories(${EIGEN3_INCLUDE_DIRS})

# Use boost libraries
find_package(Boost COMPONENTS program_options REQUIRED)

# Put debugging info in there so we can get stack traces.
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -rdynamic -O0")

# Generate .d files with dependency info
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -MD -MP")

# Source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h")

# Target
add_executable(minimu9 ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(minimu9 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link libraries
target_link_libraries(minimu9 Boost::program_options)

# Installation
install(TARGETS minimu9 DESTINATION bin)

# Generate .h.gch files
foreach(HEADER ${HEADERS})
    get_filename_component(HEADER_NAME ${HEADER} NAME)
    set_source_files_properties(${HEADER} PROPERTIES OBJECT_DEPENDS ${HEADER})
endforeach()
