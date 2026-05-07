# poc/helloworld_IAT/loader/CMakeLists.txt

PROJECT(GFL_Loader_PoC)
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

# ===== Step 1 : Set Flags =====
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s -O2")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s -O2 -std=c++11")

# ===== Step 2 : Set Application Name =====
SET(APP_NAME "gfl_loader")

# ===== Step 3 : Exe output path =====
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# ===== Step 4 : Add Subdirectory =====
ADD_SUBDIRECTORY(src)