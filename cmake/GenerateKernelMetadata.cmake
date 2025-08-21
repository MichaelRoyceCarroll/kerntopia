# Generate kernel compilation metadata
# Called during build to create JSON metadata for each compiled kernel

# Get file hashes
if(EXISTS ${SOURCE_FILE})
    file(SHA256 ${SOURCE_FILE} INPUT_HASH)
else()
    set(INPUT_HASH "unknown")
endif()

if(EXISTS ${OUTPUT_FILE})
    file(SHA256 ${OUTPUT_FILE} OUTPUT_HASH)
    file(SIZE ${OUTPUT_FILE} OUTPUT_SIZE)
else()
    set(OUTPUT_HASH "unknown")
    set(OUTPUT_SIZE 0)
endif()

# Get current timestamp
string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%dT%H:%M:%SZ")

# Get build environment info
set(CMAKE_VERSION_INFO ${CMAKE_VERSION})
cmake_host_system_information(RESULT OS_NAME QUERY OS_NAME)
cmake_host_system_information(RESULT OS_PLATFORM QUERY OS_PLATFORM)
set(PLATFORM_INFO "${OS_NAME}-${OS_PLATFORM}")

# Generate appropriate flags based on backend
if(${BACKEND} STREQUAL "cuda")
    set(PROFILE_FLAG "-capability")
    set(ENTRY_POINT "computeMain")
    set(EXTRA_FLAGS_JSON ", \"-D\", \"CUDA_BACKEND\"")
    set(EXTRA_FLAGS "-D CUDA_BACKEND")
else()
    set(PROFILE_FLAG "-profile")
    set(ENTRY_POINT "main")
    set(EXTRA_FLAGS_JSON "")
    set(EXTRA_FLAGS "")
endif()

# Generate JSON metadata
set(METADATA_CONTENT "{
  \"kernel_name\": \"${KERNEL_NAME}\",
  \"slang_version\": \"${SLANG_VERSION}\",
  \"compilation_timestamp\": \"${BUILD_TIMESTAMP}\",
  \"backend\": \"${BACKEND}\",
  \"profile\": \"${PROFILE}\",
  \"target\": \"${TARGET}\",
  \"slang_flags\": [\"-target\", \"${TARGET}\", \"${PROFILE_FLAG}\", \"${PROFILE}\", \"-stage\", \"compute\", \"-entry\", \"${ENTRY_POINT}\"${EXTRA_FLAGS_JSON}],
  \"source_file\": \"${SOURCE_FILE}\",
  \"output_file\": \"${OUTPUT_FILE}\",
  \"input_hash\": \"${INPUT_HASH}\",
  \"output_hash\": \"${OUTPUT_HASH}\",
  \"output_size\": ${OUTPUT_SIZE},
  \"build_environment\": {
    \"cmake_version\": \"${CMAKE_VERSION_INFO}\",
    \"platform\": \"${PLATFORM_INFO}\"
  },
  \"educational_notes\": {
    \"manual_compilation\": \"slangc -target ${TARGET} ${PROFILE_FLAG} ${PROFILE} -stage compute -entry ${ENTRY_POINT} ${EXTRA_FLAGS} -o ${OUTPUT_FILE} ${SOURCE_FILE}\",
    \"target_description\": \"${TARGET} bytecode for ${BACKEND} backend with ${PROFILE} capability\"
  }
}")

# Write metadata file
file(WRITE ${METADATA_FILE} "${METADATA_CONTENT}")
message(STATUS "Generated metadata: ${METADATA_FILE}")