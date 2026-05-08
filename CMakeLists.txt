# CMakeLists.txt

CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

PROJECT(GFL_Linux_Loader C CXX ASM)

# Compiler flags
SET(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -s -O2")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -O2 -std=c++11")

# Enable assembly support
ENABLE_LANGUAGE(ASM)

# Output directory for executables
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# Subdirectories
ADD_SUBDIRECTORY(src)