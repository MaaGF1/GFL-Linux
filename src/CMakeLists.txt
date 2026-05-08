# Gather source files
SET(SOURCES
    main.cpp
    autogen_stubs.S
)

# Define executable
ADD_EXECUTABLE(gfl_loader ${SOURCES})

# Compiler definitions and flags for the target
TARGET_COMPILE_OPTIONS(gfl_loader PRIVATE
    -fPIE
)

# Linker flags
TARGET_LINK_OPTIONS(gfl_loader PRIVATE
    -no-pie
)

# Link libraries (pthread for future use, also for the loader itself)
TARGET_LINK_LIBRARIES(gfl_loader PRIVATE
    pthread
)

# Include directories (if needed in future)
TARGET_INCLUDE_DIRECTORIES(gfl_loader PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})