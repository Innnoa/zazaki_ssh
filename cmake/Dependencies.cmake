include(FetchContent)

set(ZSSH_YAML_CPP_GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf)
set(ZSSH_NLOHMANN_JSON_GIT_TAG 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03)
set(ZSSH_GOOGLETEST_GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59)

function(zssh_create_interface_alias alias_target backing_target)
  if(TARGET "${backing_target}" AND NOT TARGET "${alias_target}")
    add_library(${alias_target} INTERFACE IMPORTED GLOBAL)
    set_target_properties(${alias_target} PROPERTIES INTERFACE_LINK_LIBRARIES "${backing_target}")
  endif()
endfunction()

function(zssh_require_libssh)
  if(TARGET libssh::libssh)
    return()
  endif()

  find_package(libssh CONFIG QUIET)
  if(libssh_FOUND)
    if(TARGET ssh)
      zssh_create_interface_alias(libssh::libssh ssh)
    endif()

    if(NOT TARGET libssh::libssh)
      message(FATAL_ERROR "libssh package was found, but it did not provide a usable libssh::libssh target")
    endif()

    message(STATUS "Using libssh from installed package")
    return()
  endif()

  message(FATAL_ERROR "libssh is required. Install the libssh development package or set libssh_DIR to a libssh CMake package.")
endfunction()

function(zssh_require_yaml_cpp)
  if(TARGET yaml-cpp::yaml-cpp)
    return()
  endif()

  find_package(yaml-cpp CONFIG QUIET)
  if(yaml-cpp_FOUND)
    if(TARGET yaml-cpp)
      zssh_create_interface_alias(yaml-cpp::yaml-cpp yaml-cpp)
    endif()

    if(NOT TARGET yaml-cpp::yaml-cpp)
      message(FATAL_ERROR "yaml-cpp package was found, but it did not provide a usable yaml-cpp::yaml-cpp target")
    endif()

    message(STATUS "Using yaml-cpp from installed package")
    return()
  endif()

  message(STATUS "yaml-cpp package not found; fetching yaml-cpp")
  set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
  set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG ${ZSSH_YAML_CPP_GIT_TAG}
    GIT_SHALLOW FALSE
  )
  FetchContent_MakeAvailable(yaml-cpp)
  if(TARGET yaml-cpp)
    zssh_create_interface_alias(yaml-cpp::yaml-cpp yaml-cpp)
  endif()

  if(NOT TARGET yaml-cpp::yaml-cpp)
    message(FATAL_ERROR "Fetched yaml-cpp, but it did not provide a usable yaml-cpp::yaml-cpp target")
  endif()
endfunction()

function(zssh_require_nlohmann_json)
  if(TARGET nlohmann_json::nlohmann_json)
    return()
  endif()

  find_package(nlohmann_json CONFIG QUIET)
  if(nlohmann_json_FOUND)
    if(NOT TARGET nlohmann_json::nlohmann_json)
      message(FATAL_ERROR "nlohmann_json package was found, but it did not provide a usable nlohmann_json::nlohmann_json target")
    endif()

    message(STATUS "Using nlohmann_json from installed package")
    return()
  endif()

  message(STATUS "nlohmann_json package not found; fetching nlohmann_json")
  set(JSON_BuildTests OFF CACHE INTERNAL "" FORCE)
  set(JSON_Install OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG ${ZSSH_NLOHMANN_JSON_GIT_TAG}
    GIT_SHALLOW FALSE
  )
  FetchContent_MakeAvailable(nlohmann_json)

  if(NOT TARGET nlohmann_json::nlohmann_json)
    message(FATAL_ERROR "Fetched nlohmann_json, but it did not provide a usable nlohmann_json::nlohmann_json target")
  endif()
endfunction()

function(zssh_require_googletest)
  if(TARGET GTest::gtest_main)
    return()
  endif()

  find_package(GTest CONFIG QUIET)
  if(GTest_FOUND AND TARGET GTest::gtest_main)
    message(STATUS "Using GTest from installed package")
    return()
  endif()

  message(STATUS "GTest package not found; fetching googletest")
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG ${ZSSH_GOOGLETEST_GIT_TAG}
    GIT_SHALLOW FALSE
  )
  FetchContent_MakeAvailable(googletest)

  if(NOT TARGET GTest::gtest_main)
    message(FATAL_ERROR "Fetched googletest, but it did not provide GTest::gtest_main")
  endif()
endfunction()
