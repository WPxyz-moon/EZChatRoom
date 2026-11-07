include(D:/Code/Qt/WindowsChatRoomClient/WinChatClient/Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug/.qt/QtDeploySupport.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/WinChatClient-plugins.cmake" OPTIONAL)
set(__QT_DEPLOY_ALL_MODULES_FOUND_VIA_FIND_PACKAGE "ZlibPrivate;EntryPointPrivate;Core;Gui;Widgets")

qt6_deploy_runtime_dependencies(
    EXECUTABLE D:/Code/Qt/WindowsChatRoomClient/WinChatClient/Client/build/Desktop_Qt_6_5_3_MinGW_64_bit-Debug/WinChatClient.exe
    GENERATE_QT_CONF
)
