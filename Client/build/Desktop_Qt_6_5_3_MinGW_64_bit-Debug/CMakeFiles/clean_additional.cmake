# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\WinChatClient_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\WinChatClient_autogen.dir\\ParseCache.txt"
  "WinChatClient_autogen"
  )
endif()
