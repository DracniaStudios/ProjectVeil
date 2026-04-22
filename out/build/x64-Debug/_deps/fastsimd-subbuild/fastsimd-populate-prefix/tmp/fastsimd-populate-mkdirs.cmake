# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-src")
  file(MAKE_DIRECTORY "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-src")
endif()
file(MAKE_DIRECTORY
  "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-build"
  "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix"
  "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix/tmp"
  "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix/src/fastsimd-populate-stamp"
  "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix/src"
  "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix/src/fastsimd-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix/src/fastsimd-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/GitHub/ProjectVeil/out/build/x64-Debug/_deps/fastsimd-subbuild/fastsimd-populate-prefix/src/fastsimd-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
