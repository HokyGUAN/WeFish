set(PROG_NAME client)

include_directories(
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/${PROG_NAME}
)

aux_source_directory(
  ${CMAKE_SOURCE_DIR}/${PROG_NAME}
  src_dir
)

add_executable(${PROG_NAME} ${src_dir})
