include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
message(STATUS "Local .env.cmake: ${LOCAL_ENV}")

cmake_minimum_required(VERSION 3.11.0)

set(NAME vulkan_engine)

message(STATUS "using ${CMAKE_GENERATOR}")
if (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    if (NOT MINGW_PATH) 
      message(FATAL_ERROR "MINGW_PATH not set in .env.cmake")
    endif()
    set(USE_MINGW "True")
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/gcc.exe)
    set(CMAKE_CXX_COMPILER  ${MINGW_PATH}/bin/g++.exe)
endif()

project(${NAME} VERSION 0.1)

# 1. Set VULKAN_SDK_PATH in .env.cmake to target specific vulkan version
if (DEFINED VULKAN_SDK_PATH)
  set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/include") # 1.1 Make sure this include path is correct
  set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}") # 1.2 Make sure lib path is correct
  set(Vulkan_FOUND "True")
else()
  find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
  message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()
if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using vulkan lib at: ${Vulkan_LIBRARIES}")
endif()


# 2. Set SDL2_PATH in .env.cmake to target specific sdl2
if (DEFINED SDL2_PATH)
  message(STATUS "Using specified SDL2 path: ${SDL2_PATH}")
  set(SDL2_INCLUDE_DIRS "${SDL2_PATH}/include")
  if (MSVC)
    set(SDL2_LIB "${SDL2_PATH}/windows/lib-vc2019") # 2.1 Update lib-vc2019 to use same version as your visual studio
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    message(STATUS "USING MINGW")
    set(SDL2_LIB "${SDL2_PATH}/lib-mingw-w64") # 2.1 make sure matches SDL2 mingw subdirectory
  else()
    set(SDL2_LIB "${SDL2_PATH}/macOS/lib")
  endif()
else()
  find_package(sdl2 2.24 REQUIRED)
  set(SDL2_LIB sdl2)
  message(STATUS "Found SDL2")
endif()
if (NOT SDL2_LIB)
	message(FATAL_ERROR "Could not find SDL2 library!")
else()
	message(STATUS "Using SDL2 lib at: ${SDL2_LIB}")
endif()

include_directories(external)


file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/source/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

#set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

if (WIN32)
  message(STATUS "CREATING BUILD FOR WINDOWS")

  if (USE_MINGW)
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/include
    )
    target_link_directories(${PROJECT_NAME} PUBLIC
      ${MINGW_PATH}/lib
    )
  endif()

  target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/source
    ${PROJECT_SOURCE_DIR}/source/include
    ${PROJECT_SOURCE_DIR}/source/lib
    ${Vulkan_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${FTYPE_PATH}/include
    ${UTIL_PATH}
    )

  target_link_directories(${PROJECT_NAME} PUBLIC
    ${Vulkan_LIBRARIES}/windows/lib
    ${SDL2_LIB}
    ${FTYPE_PATH}/windows/lib
  )

  target_link_libraries(${PROJECT_NAME} sdl2 freetype vulkan-1)
elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")
    target_include_directories(${PROJECT_NAME} PUBLIC
      ${PROJECT_SOURCE_DIR}/source
      ${PROJECT_SOURCE_DIR}/source/include
      ${PROJECT_SOURCE_DIR}/source/lib
      ${Vulkan_INCLUDE_DIRS}
      ${SDL2_INCLUDE_DIRS}
      ${FTYPE_PATH}/include
      ${UTIL_PATH}
    )

    if (APPLE)
      target_link_directories(${PROJECT_NAME} PUBLIC
        ${Vulkan_LIBRARIES}/macOS/lib
        ${SDL2_LIB}
        ${FTYPE_PATH}/macOS/lib
      )
      target_link_libraries(${PROJECT_NAME} libSDL2-2.0.0.dylib libSDL2_image-2.0.0.dylib libfreetype.6.dylib libvulkan.1.2.189.dylib)
    endif()
endif()