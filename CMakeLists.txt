cmake_minimum_required(VERSION 2.8)
project(crc32_check_deamon)
add_executable(${PROJECT_NAME} "main.c" "daemon.c" "crc32.cpp" "file_repo.cpp")
target_link_libraries(${PROJECT_NAME} rt)
