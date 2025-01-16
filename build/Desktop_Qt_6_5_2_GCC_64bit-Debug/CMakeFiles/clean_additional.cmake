# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles/speech_recognition_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/speech_recognition_autogen.dir/ParseCache.txt"
  "speech_recognition_autogen"
  )
endif()
