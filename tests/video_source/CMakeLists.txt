if(NOT TARGET libsmacker)
    add_subdirectory(${CMAKE_SOURCE_DIR}/LIB386/libsmacker ${CMAKE_CURRENT_BINARY_DIR}/libsmacker)
endif()
function(add_video_source_test name decoder)
    add_executable(${name} test_video_source.cpp
        ${CMAKE_SOURCE_DIR}/SOURCES/VIDEO_SOURCE.CPP)
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/LIB386/libsmacker
        ${CMAKE_SOURCE_DIR}/SOURCES ${CMAKE_SOURCE_DIR}/LIB386/H)
    target_link_libraries(${name} PRIVATE ${decoder})
    set_target_properties(${name} PROPERTIES CXX_STANDARD 11)
    add_test(NAME ${name} COMMAND ${name})
    register_host_test(${name})
endfunction()

add_video_source_test(test_video_source libsmacker)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_library(video_source_unsigned_smacker STATIC
        ${CMAKE_SOURCE_DIR}/LIB386/libsmacker/smacker.c
        ${CMAKE_SOURCE_DIR}/LIB386/libsmacker/smk_bitstream.c
        ${CMAKE_SOURCE_DIR}/LIB386/libsmacker/smk_hufftree.c
        ${CMAKE_SOURCE_DIR}/LIB386/libsmacker/smacker_compat.c)
    target_include_directories(video_source_unsigned_smacker PRIVATE
        ${CMAKE_SOURCE_DIR}/LIB386/libsmacker)
    target_compile_options(video_source_unsigned_smacker PRIVATE -funsigned-char)
    add_video_source_test(test_video_source_unsigned_char video_source_unsigned_smacker)
    target_compile_options(test_video_source_unsigned_char PRIVATE -funsigned-char)
endif()
