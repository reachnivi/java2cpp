# Crash lab: each NN_*/main.cpp builds to `labNN`.
# Debug info is always on (-g) so cores are readable; most labs are -O0.
file(GLOB LAB_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/[0-9][0-9]_*)
list(SORT LAB_DIRS)

foreach(lab ${LAB_DIRS})
  string(SUBSTRING ${lab} 0 2 num)
  set(target lab${num})
  add_executable(${target} ${lab}/main.cpp)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/common)
  target_link_libraries(${target} PRIVATE Threads::Threads)
  target_compile_options(${target} PRIVATE -g -O0)
endforeach()

# Lab 15: the same kind of crash you'll get from a production build:
# optimised (-O2), then stripped, with debug info kept in a separate file.
target_compile_options(lab15 PRIVATE -O2)
add_custom_command(TARGET lab15 POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} --only-keep-debug $<TARGET_FILE:lab15> $<TARGET_FILE:lab15>.debug
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lab15> $<TARGET_FILE:lab15>_stripped
  COMMAND ${CMAKE_STRIP} --strip-all $<TARGET_FILE:lab15>_stripped
  COMMENT "lab15: producing lab15_stripped + lab15.debug")

# Lab 16: -rdynamic exports function names so backtrace_symbols() can print them.
target_link_options(lab16 PRIVATE -rdynamic)
