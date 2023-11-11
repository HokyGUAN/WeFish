SET(PROG_NAME server)

INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/common
  ${CMAKE_SOURCE_DIR}/crypto
  ${CMAKE_SOURCE_DIR}/sbase
  ${CMAKE_SOURCE_DIR}/${PROG_NAME}
)

LINK_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/sbase
)

AUX_SOURCE_DIRECTORY(
  ${CMAKE_SOURCE_DIR}/${PROG_NAME}
  src_dir
)

ADD_EXECUTABLE(${PROG_NAME} ${src_dir})

#target_include_directories(${PROG_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/cryptopp)

TARGET_LINK_LIBRARIES(
  ${PROG_NAME} PUBLIC
  cryptor
  cryptopp
  sbase
  sqlite3
)
