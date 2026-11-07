cmake_minimum_required(VERSION 3.19)
project(WinChatClient LANGUAGES CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets LinguistTools)

qt_standard_project_setup()

qt_add_executable(WinChatClient
    WIN32 MACOSX_BUNDLE
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

# 让 WinChatClient 能找到 Base 目录下的头文件
target_include_directories(WinChatClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base
)

# 客户端与服务器共用同一份协议层（Base/），编解码逻辑只有一份。
# Base/codec.cpp、codec_pack.h 已改为通过 Base/Endian.h 做字节序转换，
# 不再依赖 <arpa/inet.h>、<endian.h>，Windows 下可以直接编译。
# 注意：Base/InetAddr.h、Md5.cpp 仍是 Linux/文件 API 专用，客户端的 MD5 用 QCryptographicHash。
target_sources(WinChatClient PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/Package.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/Messages.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/FileMessages.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/Endian.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/codec.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/codec_pack.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/codec.cpp
)

qt_add_translations(WinChatClient
    TS_FILES WinChatClient_zh_CN.ts
)

target_link_libraries(WinChatClient
    PRIVATE
        Qt::Core
        Qt::Widgets
)

install(TARGETS WinChatClient
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET WinChatClient
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
