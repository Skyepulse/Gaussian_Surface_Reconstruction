cmake_minimum_required(VERSION 3.16)
project(off_viewer CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# IGL
set(LIBIGL_WITH_OPENGL        ON  CACHE BOOL "" FORCE)
set(LIBIGL_WITH_OPENGL_GLFW   ON  CACHE BOOL "" FORCE)
set(LIBIGL_STB                ON  CACHE BOOL "" FORCE)

FetchContent_Declare(
  libigl
  GIT_REPOSITORY https://github.com/libigl/libigl.git
  GIT_TAG        v2.6.0
)

FetchContent_MakeAvailable(libigl)

# POLYSCOPE
set(POLYSCOPE_BACKEND_OPENGL3_GLFW ON CACHE BOOL "Use OpenGL3/GLFW" FORCE)
FetchContent_Declare(
  polyscope
  GIT_REPOSITORY https://github.com/nmwsharp/polyscope.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(polyscope)

set(SRC_FILES
  src/main.cpp
  src/off_cleaning/off_cleaning.cpp
  src/off_cleaning/off_cleaning.h
  src/mesh_load/mesh_load.cpp
  src/mesh_load/mesh_load.h
  src/mesh.cpp
  src/mesh.hpp
)

set(UNIT_SRC_FILES
  src/unitTests.cpp
  src/off_cleaning/off_cleaning.cpp
  src/off_cleaning/off_cleaning.h
  src/mesh_load/mesh_load.cpp
  src/mesh_load/mesh_load.h
  src/mesh.cpp
  src/mesh.hpp
)

add_executable(off_viewer ${SRC_FILES})
target_link_libraries(off_viewer
  PRIVATE
    polyscope
    igl::core
    igl::stb
)

# GoogleTest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.17.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(unit_tests ${UNIT_SRC_FILES} )
target_link_libraries(unit_tests
  PRIVATE
    polyscope
    igl::core
    igl::stb
    GTest::gtest
)

target_compile_definitions(unit_tests
  PRIVATE 
  PROJECT_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
)

add_test(NAME unit_tests
    COMMAND unit_tests
)

# On Linux they are sometimes required
find_package(Threads REQUIRED)
target_link_libraries(off_viewer PRIVATE ${OPENGL_gl_LIBRARY} Threads::Threads)
