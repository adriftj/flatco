file(GLOB_RECURSE flatco_test_sources *.cpp)

file(GLOB_RECURSE flatco_test_cxxsources *.cxx)
set(flatco_test_cxxcppfiles)
foreach(_file ${flatco_test_cxxsources})
  get_filename_component(file_name ${_file} NAME)
  set(file_cpp "${CMAKE_CURRENT_BINARY_DIR}/${file_name}.cpp")
  add_custom_command(
    OUTPUT ${file_cpp}
    DEPENDS ${_file}
    COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<$<CONFIG:Debug>:Debug>$<$<CONFIG:Release>:Release>/flatco -o ${file_cpp} ${_file}
  )
  list(APPEND flatco_test_cxxcppfiles ${file_cpp})
endforeach()

add_executable(flatco_test ${flatco_test_sources} ${flatco_test_cxxcppfiles})
