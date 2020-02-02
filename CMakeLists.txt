cmake_minimum_required (VERSION 2.6)

SET(GCC_COVERAGE_COMPILE_FLAGS "-std=c++11 -g3 ")
SET(GCC_COVERAGE_COMPILE_FLAGS "${GCC_COVERAGE_COMPILE_FLAGS} ${GCC_PROFILER_FLAGS}")
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${GCC_COVERAGE_COMPILE_FLAGS}")

#SET(CMAKE_LINKER_PROFILER_FLAGS "-pg")
SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${GCC_COVERAGE_LINK_FLAGS} ${CMAKE_LINKER_PROFILER_FLAGS}")

set(CMAKE_BUILD_TYPE Debug)

project (nfsclisim)

add_executable(nfsclisim descriptiveenum/DescriptiveEnum.cpp logging/Logging.cpp Context.cpp main.cpp Utils.cpp xdr.cpp)
target_compile_features(nfsclisim PUBLIC cxx_std_11)

target_link_libraries(nfsclisim pthread)
