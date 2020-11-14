project(mkfw)

# Create an executable
file(GLOB SOURCES *.cpp *.c)
add_executable(${PROJECT_NAME} ${SOURCES})

# Dependencies
set(APP_DEPENDS common buffer error fx2loader)
target_link_libraries(${PROJECT_NAME} PRIVATE ${APP_DEPENDS})
