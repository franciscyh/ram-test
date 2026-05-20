cmake_minimum_required(VERSION 3.16)
project(hw_test LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
set(SOURCES
    pin_mapping.cpp
    usb_driver.cpp
    test_patterns.cpp
    ram_test_cases.cpp
    hw_test.cpp
)

# 可执行文件
add_executable(hw_test ${SOURCES})

# 包含目录
target_include_directories(hw_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Rabbit/vlfd-ffi
)

# vlfd-ffi 库路径
set(VLFD_FFI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../Rabbit/vlfd-ffi/target/release")

# 链接 vlfd_ffi 库
find_library(VLFD_FFI_LIB
    NAMES vlfd_ffi
    PATHS ${VLFD_FFI_DIR}
    NO_DEFAULT_PATH
)

if(VLFD_FFI_LIB)
    target_link_libraries(hw_test PRIVATE ${VLFD_FFI_LIB})
    message(STATUS "Found vlfd_ffi library: ${VLFD_FFI_LIB}")
else()
    message(FATAL_ERROR "vlfd_ffi library not found in ${VLFD_FFI_DIR}")
endif()

# Windows 特定：链接 ws2_32（如果 vlfd-ffi 需要）
if(WIN32)
    target_link_libraries(hw_test PRIVATE ws2_32 userenv ntdll bcrypt)
endif()

# 安装：把 DLL 复制到输出目录
if(WIN32 AND VLFD_FFI_LIB)
    set(VLFD_FFI_DLL "${VLFD_FFI_DIR}/vlfd_ffi.dll")
    if(EXISTS ${VLFD_FFI_DLL})
        add_custom_command(TARGET hw_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${VLFD_FFI_DLL} $<TARGET_FILE_DIR:hw_test>
        )
        message(STATUS "Will copy vlfd_ffi.dll to output directory")
    endif()
endif()
