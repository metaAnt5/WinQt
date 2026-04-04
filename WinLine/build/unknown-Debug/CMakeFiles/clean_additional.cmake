# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\WinLine_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\WinLine_autogen.dir\\ParseCache.txt"
  "WinLine_autogen"
  )
endif()
