cmake_minimum_required (VERSION 2.8.5)

#############################################
### Beginning of system specific settings ###
#############################################


# NOTE: For simplicity, this cmake script contains host-specific code
# and needs to be adapted for new build hosts.  In principle, it would
# be cleaner to adhere better to the CMake spirit and have these
# definitions outside this script (e.g. in a calling shell script).

## host discrimination
site_name(HOST_NAME)

## some specific settings for the Cheops cluster.
## this sets CHEOPS_POSITION to -1, if not found:
# string(FIND "${HOST_NAME}" "cheops" CHEOPS_POSITION)
# if ("${CHEOPS_POSITION}" LESS 0)
  set(CHEOPS_FOUND "FALSE")
# else ()
  # message("Building on Cheops")
  # set(CHEOPS_FOUND "TRUE")
# endif ()

## some specific settings for the Jureca cluster.
# set (FZ_JUELICH_ID_FILE "/etc/FZJ/systemname")
# if (EXISTS "${FZ_JUELICH_ID_FILE}")
# file (STRINGS "${FZ_JUELICH_ID_FILE}" FZ_JUELICH_SYSTEMNAME)
# if (${FZ_JUELICH_SYSTEMNAME} STREQUAL "jureca")
#   message("Building on Jureca")
#   set(JURECA_FOUND "TRUE")
# else ()
    set(JURECA_FOUND "FALSE")
#   endif ()
# else ()
#   set(JURECA_FOUND "FALSE")
# endif ()

## some specific settings for the l71 work station:
# string(FIND "${HOST_NAME}" "l71" L71_POSITION)
# if ("${L71_POSITION}" LESS 0)
  set(L71_FOUND "FALSE")
# else ()
#  message("Building on l71.thp")
#  set(L71_FOUND "TRUE")
# endif ()

## some specific settings for thp domain computers:
# execute_process(COMMAND dnsdomainname OUTPUT_VARIABLE DOMAIN_NAME)
# string(TOLOWER "${DOMAIN_NAME}" DOMAIN_NAME_LOWER)
# string(FIND "${DOMAIN_NAME_LOWER}" "thp" THP_POSITION)
# if ("${THP_POSITION}" LESS 0)
  set(THP_DOMAIN "FALSE")
# else ()
#  message("Building in the .thp domain")
#  set(THP_DOMAIN "TRUE")
# endif ()



## On the clusters use Intel compiler

option(USE_GNU_COMPILER
  "Use gcc and g++ (default)" ON)

option(USE_INTEL_COMPILER 
  "Use Intel compiler" OFF)

option(USE_CLANG_COMPILER
  "Use the clang compiler" OFF)

if (${USE_CLANG_COMPILER})
  message("Using clang/clang++ compilers")
  set(USE_GNU_COMPILER "FALSE")
  if (${USE_INTEL_COMPILER})
    message(FATAL_ERROR "Specify either clang or Intel compilers")
  endif()
  set(CMAKE_C_COMPILER "clang")
  set(CMAKE_CXX_COMPILER "clang++")
endif ()

if (${USE_INTEL_COMPILER})
  message("Using Intel icc/icpc compilers")
  set(USE_GNU_COMPILER "FALSE")
  if (${USE_CLANG_COMPILER})
    message(FATAL_ERROR "Specify either clang or Intel compilers")
  endif()
  set(CMAKE_C_COMPILER "icc")
  set(CMAKE_CXX_COMPILER "icpc")
endif ()


if (${USE_GNU_COMPILER})
  message("Using gcc/g++ compilers")
  set(CMAKE_C_COMPILER "gcc")
  set(CMAKE_CXX_COMPILER "g++")
endif ()

project (Det)


option(BUILD_TIMING
  "include timing routines into the code" OFF)
if (${BUILD_TIMING})
  add_definitions("-DTIMING")
endif ()  



include_directories("${PROJECT_SOURCE_DIR}"
  "$ENV{HOME}/include"
  "$ENV{HOME}/opt/include"
  )


# CLang THP workarounds
if (${USE_CLANG_COMPILER} AND ${THP_DOMAIN})
  add_definitions("-D__extern_always_inline=inline")
endif ()

# GCC 4.6.3 on THP workarounds
if (NOT ${USE_CLANG_COMPILER} AND ${THP_DOMAIN})
  # prevent segfaults -O3 compilation
  set(OPTIMIZATION_LEVEL "-O2")
else()
  set(OPTIMIZATION_LEVEL "-O3")  
endif ()



# Cheops, Intel compiler
if (${CHEOPS_FOUND} AND ${USE_INTEL_COMPILER})
  # set(EXTRA_GCC_LOCATION_FLAGS "-gcc-name=/opt/rrzk/compiler/gnu/4.6.4/bin/gcc")
  set(EXTRA_GCC_LOCATION_FLAGS "-gcc-name=/opt/rrzk/compiler/gnu/4.8.2/bin/gcc")
  set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${EXTRA_GCC_LOCATION_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_GCC_LOCATION_FLAGS}")
  set(EXTRA_LIBRARY_LOCATIONS "${EXTRA_LIBRARY_LOCATIONS}" "/opt/rrzk/compiler/gnu/4.8.2/lib64")
endif ()



# MPI
find_package(MPI)
# this will set (lang = C, CXX)
# MPI_<lang>_FOUND           TRUE if FindMPI found MPI flags for <lang>
# MPI_<lang>_COMPILER        MPI Compiler wrapper for <lang>
# MPI_<lang>_COMPILE_FLAGS   Compilation flags for MPI programs
# MPI_<lang>_INCLUDE_PATH    Include path(s) for MPI header
# MPI_<lang>_LINK_FLAGS      Linking flags for MPI programs
# MPI_<lang>_LIBRARIES       All libraries to link MPI programs against
if (NOT ${MPI_CXX_FOUND})
  message("No MPI CXX compiler found!")
else ()
  include_directories(${MPI_CXX_INCLUDE_PATH})
endif ()
# -> remember to link ${MPI_CXX_LIBRARIES} for the respective
#    executables
# -> remember to set additional flags ${MPI_CXX_COMPILE_FLAGS},
#    ${MPI_CXX_LINK_FLAGS} for these targets


# OpenMP
find_package(OpenMP)
# this provides ${OpenMP_C_FLAGS} and ${OpenMP_CXX_FLAGS}



# Preprocessor flags: -D definitions (disable Armadillo C++11 support,
# since we do not use it anyway and it will give less warnings on g++
# 4.6)
add_definitions(-DDSFMT -DDSFMT_MEXP=19937 -DARMA_DONT_USE_CXX11)

option(USE_DUMA
  "build with DUMA to find memory errors" OFF)
if (${USE_DUMA})
  set(EXTRA_LIBRARY_LOCATIONS "${EXTRA_LIBRARY_LOCATIONS}" "$ENV{HOME}/packages/duma_2_5_15")
  set(EXTRA_LIBRARIES "${EXTRA_LIBRARIES}" -lduma)
  include_directories("$ENV{HOME}/packages/duma_2_5_15")
  add_definitions(-DDUMA_NO_DUMA) #test
else ()
  add_definitions(-DDUMA_NO_DUMA)
endif ()

#message("${EXTRA_LIBRARY_LOCATIONS}")



# CXX flags:
# -march settings
if (${USE_CLANG_COMPILER} OR NOT ${THP_DOMAIN})
  set(MARCH_FLAGS "-march=native")
else ()
  # setting to use instead of --march=native -- to be able to run on Core2 computers in the thp domain
  # use a safer setting
  # [obtained via `cc -march=native -E -v - </dev/null 2>&1 | grep cc1` on l07]
  set(MARCH_FLAGS "-march=core2 -mcx16 -msahf -mno-movbe -mno-aes -mno-pclmul -mno-popcnt -mno-abm -mno-lwp -mno-fma -mno-fma4 -mno-xop -mno-bmi -mno-tbm -mno-avx -mno-sse4.2 -msse4.1 --param l1-cache-size=32 --param l1-cache-line-size=64 --param l2-cache-size=6144 -mtune=core2 -fstack-protector")
endif ()
set(WARNING_FLAGS "-Wall -Winit-self -Wextra -Woverloaded-virtual -Wconversion")
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   ${MARCH_FLAGS} ${WARNING_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MARCH_FLAGS} ${WARNING_FLAGS} -std=c++0x")


# build type specific flags:
# these are appended to ${CMAKE_C[XX]_FLAGS} for DEBUG / RELEASE / ETC builds

if (${USE_INTEL_COMPILER})
  set(EXTRA_FLAGS_DEBUG "-g -O0")
  # floating-point traps for division by zero, invalid operation, and overflow
else ()
  set(EXTRA_FLAGS_DEBUG "-g -O0")
endif ()
set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   ${EXTRA_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${EXTRA_FLAGS_DEBUG}")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
  # TODO SOMETIME: option for duma
  #  link_directories("$ENV{HOME}/packages/duma_2_5_15")
  #  include_directories("$ENV{HOME}/packages/duma_2_5_15")
  #  set(EXTRA_LIBRARIES ${EXTRA_LIBRARIES} duma)

  if (${CHEOPS_FOUND} AND ${USE_INTEL_COMPILER})
  # disable GLIBCXX debug version on the cluster, where we use the Intel compiler
  # -> some code using std::map::operator[] failed to build
    add_definitions(-DMAX_DEBUG)
  else ()
    add_definitions(-D_GLIBCXX_DEBUG -DMAX_DEBUG)
  endif ()
endif ()

# Note: Let's use -g also for Release builds to make core dumps more
# readable in the debugger, we do not really need separate RelWithDebInfo
if (${USE_INTEL_COMPILER})
  set(EXTRA_FLAGS_RELEASE
    "${OPTIMIZATION_LEVEL} -ansi-alias -xSSE4.2 -no-prec-div -g")
else ()
  set(EXTRA_FLAGS_RELEASE
    "${OPTIMIZATION_LEVEL} -fno-strict-aliasing -ffast-math -g")
endif ()
set(CMAKE_C_FLAGS_RELEASE   "${CMAKE_C_FLAGS_RELEASE}   ${EXTRA_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${EXTRA_FLAGS_RELEASE}")

if(CMAKE_BUILD_TYPE MATCHES "Release")
  add_definitions(-DBOOST_DISABLE_ASSERTS -DARMA_NO_DEBUG -DNDEBUG -DHAVE_SSE2)
endif ()


# set(CMAKE_C_FLAGS_RELWITHDEBINFO   "${CMAKE_C_FLAGS_RELWITHDEBINFO}   ${EXTRA_FLAGS_RELEASE} -g")
# set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${EXTRA_FLAGS_RELEASE} -g")

# if(CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
#   add_definitions(-DBOOST_DISABLE_ASSERTS -DARMA_NO_DEBUG -DNDEBUG -DHAVE_SSE2)
# endif ()




# External libraries -- Armadillo
# This uses LAPACK and BLAS or a compatible implementation like the Intel MKL.
# Armadillo can be used "header only" and for simplicity these headers are included.
include_directories("${PROJECT_SOURCE_DIR}/armadillo")
set(EXTRA_LIBRARY_LOCATIONS "${EXTRA_LIBRARY_LOCATIONS}" "$ENV{HOME}/lib" "$ENV{HOME}/opt/lib")
if (${CHEOPS_FOUND} OR ${JURECA_FOUND})
  # set(ARMADILLO_LIBRARIES
  #   "-lmkl_intel_lp64 -lmkl_gnu_thread -lmkl_core -liomp5 -lpthread")
  set(ARMADILLO_LIBRARIES
      "-lmkl_intel_lp64 -lmkl_intel_thread -lmkl_core -liomp5 -lpthread")
  # do not link -lgfortran
else ()
  # set(ARMADILLO_LIBRARIES
  #   "-llapack -lblas -lgfortran")
  set(ARMADILLO_LIBRARIES
    "-llapack -lblas")
  if (${THP_DOMAIN})
    # on thp machines: explicitly link to Lapack, not the Atlas implementation (which is not compatible to Openblas)
    set(EXTRA_LIBRARY_LOCATIONS "${EXTRA_LIBRARY_LOCATIONS}" "/usr/lib/lapack/")
  else ()
  endif ()
endif ()

# CNPY
include_directories("${PROJECT_SOURCE_DIR}/cnpy")


# FFTW3
if (${CHEOPS_FOUND} OR ${JURECA_FOUND})
  # Use Intel's MKL FFTW3 compatibility wrapper
  set(FFTW3_LIBRARIES ${ARMADILLO_LIBRARIES})
else ()
  set(FFTW3_LIBRARIES "-lfftw3")
endif ()

link_directories(${EXTRA_LIBRARY_LOCATIONS})



# For machines that have this installed and easily accessible: include
# pytools.cpp and link to -lpython2.7
option(USE_PYTOOLS
  "include some python routines for interactivity (does not work on Cheops cluster)" ON)

if (${CHEOPS_FOUND} OR ${JURECA_FOUND})
  set(USE_PYTOOLS OFF)
endif ()

if (USE_PYTOOLS)
  set(PYTOOLS_SRC "pytools.cpp")
  set(PYTHON_LIB "-lpython2.7")
else ()
  set(PYTOOLS_SRC "")
  set(PYTHON_LIB "")
endif ()



#######################################
### End of system specific settings ###
#######################################




## foreign code in subdirectories -> have a separate CMakeLists.txt for those
# directly include needed boost source files to reduce linker woes
# (debug-builts don't work unless exactly the same compiler options
# are applied to the libraries linked in...)
add_subdirectory(boost-chrono)
add_subdirectory(boost-filesystem)
add_subdirectory(boost-mpi)
add_subdirectory(boost-program-options)
add_subdirectory(boost-serialization)
add_subdirectory(boost-system)
add_subdirectory(boost-timer)
# collect all boost libraries, but keep MPI separete:
set(BOOST_LIBS
  boost_program_options boost_filesystem boost_serialization
  boost_system boost_timer boost_chrono)
# we need -lrt for timer / chrono
set(BOOST_LIBS
  ${BOOST_LIBS} "-lrt")
add_subdirectory(dsfmt)


## Git revision info
# Get the current working branch
execute_process(
  COMMAND git rev-parse --abbrev-ref HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BRANCH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
# Get the latest commit hash of the working branch
execute_process(
  COMMAND git rev-parse HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_REVISION_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)


configure_file(
  git-revision.c.in             # in source tree
  ${PROJECT_BINARY_DIR}/git-revision.c                # in binary tree
)




## Executables

# Compile all the source files that are going to be compiled into
# multiple executables in a single static library; then the linker
# will only pull out the necessary object files.  Otherwise CMake will
# recompile all source files separately for each target!
# [Unfortunately, in this respect, for this use case, CMake is
# actually less convenient than rolling one's own makefiles]
#
# We do this separately for a couple of the source files to limit
# unnecessary recompilations.

set(general_common_SRC rngwrapper.cpp metadata.cpp tools.cpp
    ${PROJECT_BINARY_DIR}/git-revision.c)
add_library(general_common ${general_common_SRC})

set(detqmc_common_SRC detqmcparams.cpp detmodel.cpp timing.cpp
   detmodelloggingparams.cpp ${PYTOOLS_SRC})
add_library(detqmc_common ${detqmc_common_SRC})

set(detqmc_nonmpi_common_SRC observablehandler.cpp)
add_library(detqmc_nonmpi_common ${detqmc_nonmpi_common_SRC})

set(dethubbard_common_SRC dethubbard.cpp dethubbardparams.cpp)
add_library(dethubbard_common ${dethubbard_common_SRC})

set(detsdw_common_SRC detsdwparams.cpp detsdwsystemconfig.cpp)
add_library(detsdw_common ${detsdw_common_SRC})

set(detsdwo1_common_SRC detsdwo1.cpp)
add_library(detsdwo1_common ${detsdwo1_common_SRC})

set(detsdwo2_common_SRC detsdwo2.cpp)
add_library(detsdwo2_common ${detsdwo2_common_SRC})

set(detsdwo3_common_SRC detsdwo3.cpp)
add_library(detsdwo3_common ${detsdwo3_common_SRC})

set(detsdwopdim_common_SRC detsdwopdim.cpp)
add_library(detsdwopdim_common ${detsdwopdim_common_SRC})



set(detqmchubbard_SRC maindetqmchubbard.cpp)
add_executable(detqmchubbard
  ${detqmchubbard_SRC})
target_link_libraries(detqmchubbard
  dethubbard_common detqmc_nonmpi_common detqmc_common general_common
  dsfmt ${ARMADILLO_LIBRARIES} ${BOOST_LIBS} ${PYTHON_LIB} ${EXTRA_LIBRARIES})

set(detqmcsdwo1_SRC maindetqmcsdwo1.cpp)
add_executable(detqmcsdwo1
  ${detqmcsdwo1_SRC})
target_link_libraries(detqmcsdwo1
  detsdwo1_common detsdw_common detqmc_nonmpi_common detqmc_common general_common
  dsfmt ${ARMADILLO_LIBRARIES} ${BOOST_LIBS} ${PYTHON_LIB} ${EXTRA_LIBRARIES})

set(detqmcsdwo2_SRC maindetqmcsdwo2.cpp)
add_executable(detqmcsdwo2
  ${detqmcsdwo2_SRC})
target_link_libraries(detqmcsdwo2
  detsdwo2_common detsdw_common detqmc_nonmpi_common detqmc_common general_common
  dsfmt ${ARMADILLO_LIBRARIES} ${BOOST_LIBS} ${PYTHON_LIB} ${EXTRA_LIBRARIES})

set(detqmcsdwo3_SRC maindetqmcsdwo3.cpp)
add_executable(detqmcsdwo3
  ${detqmcsdwo3_SRC})
target_link_libraries(detqmcsdwo3
  detsdwo3_common detsdw_common detqmc_nonmpi_common detqmc_common general_common
  dsfmt ${ARMADILLO_LIBRARIES} ${BOOST_LIBS} ${PYTHON_LIB} ${EXTRA_LIBRARIES})

set(detqmcsdwopdim_SRC maindetqmcsdwopdim.cpp)
add_executable(detqmcsdwopdim
  ${detqmcsdwopdim_SRC})
target_link_libraries(detqmcsdwopdim
  detsdwopdim_common detsdw_common detqmc_nonmpi_common detqmc_common general_common  
  dsfmt ${ARMADILLO_LIBRARIES} ${BOOST_LIBS} ${PYTHON_LIB} ${EXTRA_LIBRARIES})


if ("${MPI_CXX_FOUND}")
  set(detqmc_mpi_common_SRC mpiobservablehandlerpt.cpp
    observablehandler.cpp detqmcptparams.cpp)
  add_library(detqmc_mpi_common ${detqmc_mpi_common_SRC})

  set(PT_SIM_LIBRARIES general_common detqmc_mpi_common
    detqmc_nonmpi_common detqmc_common dsfmt ${ARMADILLO_LIBRARIES}
    boost_mpi ${BOOST_LIBS} ${PYTHON_LIB} ${MPI_CXX_LIBRARIES} ${EXTRA_LIBRARIES})

  set(detqmcptsdwo1_SRC mpimaindetqmcptsdwo1.cpp)
  add_executable(detqmcptsdwo1
    ${detqmcptsdwo1_SRC})
  target_link_libraries(detqmcptsdwo1
    detsdwo1_common detsdw_common ${PT_SIM_LIBRARIES})

  set(detqmcptsdwo2_SRC mpimaindetqmcptsdwo2.cpp)
  add_executable(detqmcptsdwo2
    ${detqmcptsdwo2_SRC})
  target_link_libraries(detqmcptsdwo2
    detsdwo2_common detsdw_common ${PT_SIM_LIBRARIES})

  set(detqmcptsdwo3_SRC mpimaindetqmcptsdwo3.cpp)
  add_executable(detqmcptsdwo3
    ${detqmcptsdwo3_SRC})
  target_link_libraries(detqmcptsdwo3
    detsdwo3_common detsdw_common ${PT_SIM_LIBRARIES})

  set(detqmcptsdwopdim_SRC mpimaindetqmcptsdwopdim.cpp)
  add_executable(detqmcptsdwopdim
    ${detqmcptsdwopdim_SRC})
  target_link_libraries(detqmcptsdwopdim
    detsdwopdim_common detsdw_common ${PT_SIM_LIBRARIES})

  # Compile / Link flags
  set(MPI_EXECUTABLES
    detqmcptsdwopdim detqmcptsdwo1 detqmcptsdwo2 detqmcptsdwo3)
  set_target_properties(${MPI_EXECUTABLES} PROPERTIES
    COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}"
    LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
  
endif ()


set(binarystreamtotext_SRC mainbinarystreamtotext.cpp)
add_executable(binarystreamtotext
  ${binarystreamtotext_SRC})
target_link_libraries(binarystreamtotext general_common ${EXTRA_LIBRARIES})


set(binarystreamtonormmeanseries_SRC mainbinarystreamtonormmeanseries.cpp)
add_executable(binarystreamtonormmeanseries
  ${binarystreamtonormmeanseries_SRC})
target_link_libraries(binarystreamtonormmeanseries general_common ${EXTRA_LIBRARIES})

set(binarystreamtonormmeanseriesrepeated_SRC mainbinarystreamtonormmeanseriesrepeated.cpp)
add_executable(binarystreamtonormmeanseriesrepeated
  ${binarystreamtonormmeanseriesrepeated_SRC})
target_link_libraries(binarystreamtonormmeanseriesrepeated general_common ${EXTRA_LIBRARIES})


set(extractfrombinarystream_SRC mainextractfrombinarystream.cpp)
add_executable(extractfrombinarystream
  ${extractfrombinarystream_SRC})
target_link_libraries(extractfrombinarystream general_common boost_program_options ${EXTRA_LIBRARIES})

set(sdwcorr_SRC mainsdwcorr.cpp cnpy/cnpy.cpp)
add_executable(sdwcorr
  ${sdwcorr_SRC})
target_link_libraries(sdwcorr general_common
  boost_program_options boost_filesystem boost_system
  "z" ${ARMADILLO_LIBRARIES} ${FFTW3_LIBRARIES} ${EXTRA_LIBRARIES})

set(sdweqtimesusc_SRC mainsdweqtimesusc.cpp)
add_executable(sdweqtimesusc
  ${sdweqtimesusc_SRC})
target_link_libraries(sdweqtimesusc general_common
  boost_program_options boost_filesystem boost_system
  ${ARMADILLO_LIBRARIES} ${EXTRA_LIBRARIES})


set(tauintsimple_SRC maintauintsimple.cpp)
add_executable(tauintsimple
  ${tauintsimple_SRC})
target_link_libraries(tauintsimple general_common ${EXTRA_LIBRARIES})

set(deteval_SRC maindeteval.cpp)
add_executable(deteval ${deteval_SRC})
target_link_libraries(deteval general_common boost_program_options boost_filesystem boost_system ${EXTRA_LIBRARIES})

set(detevalbc_SRC maindetevalbc.cpp)
add_executable(detevalbc ${detevalbc_SRC})
target_link_libraries(detevalbc general_common
  boost_program_options boost_filesystem boost_system ${EXTRA_LIBRARIES})

set(jointimeseries_SRC mainjointimeseries.cpp)
add_executable(jointimeseries ${jointimeseries_SRC})
target_link_libraries(jointimeseries general_common
  boost_program_options boost_filesystem boost_system ${EXTRA_LIBRARIES})


# set(testspheresampling_SRC maintestspheresampling.cpp)
# add_executable(testspheresampling ${testspheresampling_SRC})
# target_link_libraries(testspheresampling general_common
#   ${ARMADILLO_LIBRARIES} dsfmt ${EXTRA_LIBRARIES})


## Simon Trebst's code for feedback optimization of parallel
## tempering control parameter value distributions, not included
## in this distribution.

# add_executable(feedback feedback.cc)


set(mrpt_COMMON_SRC
  mrpt.cpp
  mrpt-jk.cpp
  statistics.cpp
  )
add_library(mrpt_COMMON ${mrpt_COMMON_SRC})
add_library(mrpt_HIGHLEVEL mrpt-highlevel.cpp)
add_executable(mrpt main-mrpt.cpp)
target_link_libraries(mrpt mrpt_HIGHLEVEL mrpt_COMMON)
add_executable(mrptbc main-mrptbc.cpp)
target_link_libraries(mrptbc mrpt_HIGHLEVEL mrpt_COMMON)
add_library(mrpt_binderRatioIntersect mrpt-binderratio-intersect.cpp)
add_executable(mrpt-binderratio-intersect main-mrpt-binderratio-intersect.cpp)
target_link_libraries(mrpt-binderratio-intersect mrpt_binderRatioIntersect mrpt_COMMON)
add_executable(mrptbc-binderratio-intersect main-mrptbc-binderratio-intersect.cpp)
target_link_libraries(mrptbc-binderratio-intersect mrpt_binderRatioIntersect mrpt_COMMON)
add_library(mrpt_findIntersect mrpt-find-intersect.cpp)
add_executable(mrpt-find-intersect main-mrpt-find-intersect.cpp)
target_link_libraries(mrpt-find-intersect mrpt_findIntersect mrpt_COMMON)
if(${OPENMP_FOUND})
  # add -fopenmp or -openmp or whatever
  set(MRPT_TARGETS
    mrpt_HIGHLEVEL mrpt_COMMON mrpt_binderRatioIntersect mrpt_findIntersect
    mrpt-binderratio-intersect mrptbc-binderratio-intersect mrpt mrptbc mrpt-find-intersect)
  foreach( target ${MRPT_TARGETS} )
    set_target_properties(${target} PROPERTIES COMPILE_FLAGS "${OpenMP_CXX_FLAGS}"
                                               LINK_FLAGS    "${OpenMP_CXX_FLAGS}")
  endforeach( target ${MRPT_TARGETS} )                                        
endif ()
target_link_libraries(mrpt general_common boost_filesystem boost_system ${EXTRA_LIBRARIES})
target_link_libraries(mrptbc general_common boost_filesystem boost_system ${EXTRA_LIBRARIES})
target_link_libraries(mrpt-binderratio-intersect general_common boost_filesystem boost_system ${EXTRA_LIBRARIES})
target_link_libraries(mrptbc-binderratio-intersect general_common boost_filesystem boost_system ${EXTRA_LIBRARIES})
target_link_libraries(mrpt-find-intersect general_common boost_filesystem boost_system ${EXTRA_LIBRARIES})




