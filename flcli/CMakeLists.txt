project(flcli)

# Create an executable
file(GLOB SOURCES *.cpp *.c)
add_executable(${PROJECT_NAME} ${SOURCES})

# Dependencies
set(APP_DEPENDS common buffer error fpgalink argtable2 dump)
target_link_libraries(${PROJECT_NAME} PRIVATE ${APP_DEPENDS})

# What to install
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
