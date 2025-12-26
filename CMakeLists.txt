if(NOT DEFINED CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation Directory")
endif()

message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build" FORCE)
endif()

project(tokenizer.axera)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

set(HF_TOKENIZER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/hf-tokenizer-sdk-aarch64 CACHE PATH "HuggingFace Tokenizer FFI Directory")

message(STATUS "HF_TOKENIZER_DIR = ${HF_TOKENIZER_DIR}")

# include 路径推荐用 target_include_directories
set(HF_TOKENIZER_INCLUDE_DIR "${HF_TOKENIZER_DIR}/include")
set(HF_TOKENIZER_LIB_DIR     "${HF_TOKENIZER_DIR}/lib")

# 你可以按实际情况选择 .a 或 .so
set(HF_TOKENIZER_LIB_STATIC "${HF_TOKENIZER_LIB_DIR}/libhf_tokenizer_ffi.a")
set(HF_TOKENIZER_LIB_SHARED "${HF_TOKENIZER_LIB_DIR}/libhf_tokenizer_ffi.so")

if (EXISTS "${HF_TOKENIZER_LIB_STATIC}")
  add_library(hf_tokenizer_ffi STATIC IMPORTED GLOBAL)
  set_target_properties(hf_tokenizer_ffi PROPERTIES
    IMPORTED_LOCATION "${HF_TOKENIZER_LIB_STATIC}"
  )
elseif (EXISTS "${HF_TOKENIZER_LIB_SHARED}")
  add_library(hf_tokenizer_ffi SHARED IMPORTED GLOBAL)
  set_target_properties(hf_tokenizer_ffi PROPERTIES
    IMPORTED_LOCATION "${HF_TOKENIZER_LIB_SHARED}"
  )
else()
  message(FATAL_ERROR "Cannot find libhf_tokenizer_ffi in ${HF_TOKENIZER_LIB_DIR}")
endif()

add_library(tokenizer SHARED src/BaseTokenizer.cpp src/tokenizer/tokenizer_hf.cpp)
target_include_directories(tokenizer PRIVATE "${HF_TOKENIZER_INCLUDE_DIR}")
target_link_libraries(tokenizer PRIVATE hf_tokenizer_ffi pthread dl)

add_library(tokenizer_static STATIC src/BaseTokenizer.cpp src/tokenizer/tokenizer_hf.cpp)
target_include_directories(tokenizer_static PRIVATE "${HF_TOKENIZER_INCLUDE_DIR}")
target_link_libraries(tokenizer_static PRIVATE hf_tokenizer_ffi pthread dl)  # ✅ 你之前这里漏了


function(add_test test_name)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} tokenizer)

    
    add_executable(${test_name}_static tests/${test_name}.cpp)
    target_link_libraries(${test_name}_static tokenizer_static)
endfunction()

# option to build tests
option(TOKENIZER_BUILD_TESTS "Build tests" ON)
MESSAGE(STATUS "TOKENIZER_BUILD_TESTS = ${TOKENIZER_BUILD_TESTS}")

if(TOKENIZER_BUILD_TESTS)
    add_test(test_tokenizer)
    add_test(test_num2words_en)
endif()


install(TARGETS tokenizer DESTINATION lib)
install(TARGETS tokenizer_static DESTINATION lib)

install(FILES ${HF_TOKENIZER_DIR}/lib/libhf_tokenizer_ffi.a DESTINATION lib)

install(FILES include/BaseTokenizer.hpp DESTINATION include)