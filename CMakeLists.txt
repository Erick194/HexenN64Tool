cmake_minimum_required(VERSION 3.25)

project(HexenN64Tool)

if(MSVC)
	add_definitions("/D_CRT_SECURE_NO_WARNINGS")
endif()

find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)

# Global identifiers for each project/target
set(HASH_LIBRARY_TGT_NAME           Hash-Library)

add_subdirectory("${PROJECT_SOURCE_DIR}/src")
add_subdirectory("${PROJECT_SOURCE_DIR}/third_party_libs/hash-library")