cmake_minimum_required(VERSION 3.16)
project(cv_pi5 C)

# Language + warnings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Put binaries in build/.../bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Public headers (add later if you expose APIs)
include_directories(${CMAKE_SOURCE_DIR}/include)

# Executable: command-triggered recorder
add_executable(cam_trigger
  src/apps/cam_trigger/main.c
)

# (Later) link your own libraries here, e.g.:
# target_link_libraries(cam_trigger PRIVATE capture vision)