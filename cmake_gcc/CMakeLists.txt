# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	free_rtos
	VERSION 1.0
	LANGUAGES C CXX ASM
)
add_compile_options(-mfp16-format=ieee)
# Include the definition of the slc_free_rtos target,
# which contains the content of the SLC project
include(free_rtos.cmake)

add_executable(free_rtos
    # Add additional sources here
    ../sensor/max30102.c
    ../tasks/ppg_task.c
    ../tasks/ai_task.c
    ../tasks/filter_task.c
    ../algo/filter.c
    ../tasks/bluetooth_data_task.c
    ../algo/vitals.c
    ../algo/app_ml.cpp
    ../ble/ead.c
    ../ble/bl_bt_event.c
    ../ble/ble_button.c
    ../ble/ble_log.c
    ../ble/timer_cb.c
)

target_include_directories(free_rtos PUBLIC
    # Add additional include paths here
)

target_compile_definitions(free_rtos PUBLIC
    # Add additional macros here
)

target_compile_options(free_rtos PUBLIC
    # Set additional compiler flags here
)

target_link_options(free_rtos PUBLIC
    # Set additional linker flags here
    -u _printf_float
)

# Link with the content defined in the SLC project
target_link_libraries(free_rtos PRIVATE
    slc
)

# Include managed project content if available
include(free_rtos_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(free_rtos PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET free_rtos
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:free_rtos>" "$<TARGET_FILE_DIR:free_rtos>/$<TARGET_FILE_BASE_NAME:free_rtos>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:free_rtos>" "$<TARGET_FILE_DIR:free_rtos>/$<TARGET_FILE_BASE_NAME:free_rtos>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:free_rtos>" "$<TARGET_FILE_DIR:free_rtos>/$<TARGET_FILE_BASE_NAME:free_rtos>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET free_rtos
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()