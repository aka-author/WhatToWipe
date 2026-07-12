# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles\\EraseAndRewrite_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\EraseAndRewrite_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\phase1_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\phase1_tests_autogen.dir\\ParseCache.txt"
  "CMakeFiles\\phase2_tests_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\phase2_tests_autogen.dir\\ParseCache.txt"
  "EraseAndRewrite_autogen"
  "phase1_tests_autogen"
  "phase2_tests_autogen"
  )
endif()
