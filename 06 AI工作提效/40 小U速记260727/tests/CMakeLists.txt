cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Sql Test Widgets)

add_executable(test_storage
    test_storage.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/database.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/notestorage.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/todostorage.cpp
    ${CMAKE_SOURCE_DIR}/src/storage/tagstorage.cpp
    ${CMAKE_SOURCE_DIR}/src/services/backupservice.cpp
)

target_include_directories(test_storage PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/storage
)

target_link_libraries(test_storage PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
    Qt6::Widgets
)

add_test(NAME test_storage COMMAND test_storage)