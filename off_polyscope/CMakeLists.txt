cmake_minimum_required(VERSION 3.16)
project(off_viewer CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Eigen (header-only; system install)
find_package(Eigen3 3.3 REQUIRED NO_MODULE)

# Polyscope via FetchContent
include(FetchContent)
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
)

add_executable(off_viewer ${SRC_FILES})
target_link_libraries(off_viewer
  PRIVATE
    polyscope
    Eigen3::Eigen
)

# On Linux sometimes you need these for GL/GUI
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
target_link_libraries(off_viewer PRIVATE ${OPENGL_gl_LIBRARY} Threads::Threads)
