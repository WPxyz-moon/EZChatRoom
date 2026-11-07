cmake_minimum_required(VERSION 2.8.0)
#设置编译器
set(CMAKE_C_COMPILER "/usr/local/gcc-9.3.0/bin/gcc")
set(CMAKE_CXX_COMPILER "/usr/local/gcc-9.3.0/bin/g++")

project(test)

# 设置C++配置
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置默认文件存放位置
# CMAKE_BINARY_DIR是 cmake执行的位置
# CMAKE_SOURCE_DIR是 顶层CMakeLists.txt所在位置
# CMAKE_CURRENT_BINARY_DIR--①
# CMAKE_CURRENT_SOURCE_DIR是当前CMakeLists.txt所在我文件位置
# ①--CMAKE构建多级目录时，会在cmake执行目录（build）生成和CMake文件同样的目录结构。
# 比如说 总Cmake下 a/ 和 b/ 下分别有CMake文件，build内就会有 a/ b/
# 而CURRENT_BINARY_DIR 就是build内部的 a/(如果当前执行到 主目录/a/的Cmake了)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


add_executable(ezChatRoom
    ${CMAKE_SOURCE_DIR}/ChatRoom/main.cc
    ${CMAKE_SOURCE_DIR}/Base/codec.cpp
    ${CMAKE_SOURCE_DIR}/Base/Md5.cpp
    ${CMAKE_SOURCE_DIR}/ChatRoom/ChatRoom.cc
    ${CMAKE_SOURCE_DIR}/ChatRoom/FileManager.cc
    ${CMAKE_SOURCE_DIR}/Reactor/Acceptor.cc
    ${CMAKE_SOURCE_DIR}/Reactor/Connection.cc
    ${CMAKE_SOURCE_DIR}/Reactor/EventLoop.cc
    ${CMAKE_SOURCE_DIR}/Reactor/KeyBoardHandler.cc
)
target_include_directories(ezChatRoom PRIVATE 
                        ${CMAKE_SOURCE_DIR}/ChatRoom 
                        ${CMAKE_SOURCE_DIR}/Base
                        ${CMAKE_SOURCE_DIR}/Reactor
)



