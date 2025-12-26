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

include_directories(${HF_TOKENIZER_DIR}/include)
link_directories(${HF_TOKENIZER_DIR}/lib)

add_library(tokenizer SHARED src/BaseTokenizer.cpp src/tokenizer/tokenizer_hf.cpp)
target_link_libraries(tokenizer hf_tokenizer_ffi pthread dl)

add_library(tokenizer_static STATIC src/BaseTokenizer.cpp src/tokenizer/tokenizer_hf.cpp)


function(add_test test_name)
    add_executable(${test_name} tests/${test_name}.cpp)
    target_link_libraries(${test_name} tokenizer)

    
    add_executable(${test_name}_static tests/${test_name}.cpp)
    target_link_libraries(${test_name}_static tokenizer_static hf_tokenizer_ffi pthread dl)
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