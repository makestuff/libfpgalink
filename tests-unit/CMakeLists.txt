# Create a test-driver executable
file(GLOB SOURCES *.cpp *.c ../src/*.cpp ../src/*.c)
add_executable(${PROJECT_NAME}-tests ${SOURCES})

# Link with GoogleTest and the library's dependencies
target_include_directories(${PROJECT_NAME}-tests PRIVATE ../include ../src)
target_link_libraries(${PROJECT_NAME}-tests PRIVATE gtest gmock gtest_main ${LIB_DEPENDS})

# Add this test-driver
add_test(
  NAME ${PROJECT_NAME}-tests
  COMMAND ${PROJECT_NAME}-tests --gtest_output=xml:${CMAKE_BINARY_DIR}/test-results/${PROJECT_NAME}.xml
)
