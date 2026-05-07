# poc/helloworld_IAT/loader/src/CMakeLists.txt

FILE(GLOB_RECURSE SRC_LIST ./*.cpp)

ADD_EXECUTABLE(${APP_NAME} ${SRC_LIST})

# Only link standard libraries needed for memory mapping
TARGET_LINK_LIBRARIES(${APP_NAME} pthread)