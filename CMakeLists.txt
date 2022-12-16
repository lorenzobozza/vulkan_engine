include(./.env.cmake OPTIONAL RESULT_VARIABLE LOCAL_ENV)
if( DEFINED LOCAL_ENV)
  message(STATUS "Found local .env.cmake at: ${LOCAL_ENV}")
endif()

cmake_minimum_required(VERSION 3.11.0)

set(NAME vulkan_engine)

message(STATUS "Using ${CMAKE_GENERATOR}")

# If using MinGW Makefile, set compiler paths as given
if (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    if (NOT MINGW_PATH) 
      message(FATAL_ERROR "MINGW_PATH not set in .env.cmake")
    endif()
    set(USE_MINGW "True")
    set(CMAKE_C_COMPILER ${MINGW_PATH}/bin/gcc.exe)
    set(CMAKE_CXX_COMPILER  ${MINGW_PATH}/bin/g++.exe)
endif()

project(${NAME} VERSION 0.6)

# Set VULKAN_SDK_PATH in .env.cmake or cmake call to target specific Vulkan version
if (DEFINED VULKAN_SDK_PATH)
  set(Vulkan_INCLUDE_DIRS "${VULKAN_SDK_PATH}/include")
  if (MSVC)
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/windows/lib")
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/windows/lib")
  else()
    set(Vulkan_LIBRARIES "${VULKAN_SDK_PATH}/macOS/lib")
  endif()
  set(Vulkan_FOUND "True")
else()
  find_package(Vulkan REQUIRED) # throws error if could not find Vulkan
  message(STATUS "Found Vulkan: $ENV{VULKAN_SDK}")
endif()

if (NOT Vulkan_FOUND)
	message(FATAL_ERROR "Could not find Vulkan library!")
else()
	message(STATUS "Using Vulkan lib at: ${Vulkan_LIBRARIES}")
endif()


# Set SDL2_PATH in .env.cmake or cmake call to target specific SDL2 version
if (DEFINED SDL2_PATH)
  if (MSVC)
    set(SDL2_INCLUDE_DIRS "${SDL2_PATH}/windows/include")
    set(SDL2_LIB "${SDL2_PATH}/windows/lib-vc2019")
    file(GLOB SDL2_DLL "${SDL2_PATH}/windows/lib-vc2019/*.dll")
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    set(SDL2_INCLUDE_DIRS "${SDL2_PATH}/windows/include")
    set(SDL2_LIB "${SDL2_PATH}/windows/lib-mingw-w64")
    file(GLOB SDL2_DLL "${SDL2_PATH}/windows/lib-mingw-w64/*.dll")
  else()
    set(SDL2_INCLUDE_DIRS "${SDL2_PATH}/macOS/include")
    set(SDL2_LIB "${SDL2_PATH}/macOS/lib")
  endif()
else()
  find_package(SDL2 2.24 REQUIRED) # throws error if could not find SDL2
  set(SDL2_LIB SDL2)
  message(STATUS "Found SDL2")
endif()

if (NOT SDL2_LIB)
	message(FATAL_ERROR "Could not find SDL2 library!")
else()
	message(STATUS "Using SDL2 lib at: ${SDL2_LIB}")
endif()

# Set FTYPE_PATH in .env.cmake or cmake call to target specific FREETYPE version
if (DEFINED FTYPE_PATH)
  set(FTYPE_INCLUDE_DIRS "${FTYPE_PATH}/include")
  if (MSVC)
    set(FTYPE_LIB "${FTYPE_PATH}/windows/lib")
  elseif (CMAKE_GENERATOR STREQUAL "MinGW Makefiles")
    set(FTYPE_LIB "${FTYPE_PATH}/windows/lib")
  else()
    set(FTYPE_LIB "${FTYPE_PATH}/macOS/lib")
  endif()
else()
  find_package(freetype REQUIRED) # throws error if could not find Freetype
  set(FTYPE_LIB freetype)
  message(STATUS "Found Freetype")
endif()

if (NOT FTYPE_LIB)
	message(FATAL_ERROR "Could not find Freetype library!")
else()
	message(STATUS "Using Freetype lib at: ${FTYPE_LIB}")
endif()

include_directories(external)

file(GLOB_RECURSE SOURCES ${PROJECT_SOURCE_DIR}/source/*.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)

#set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

target_include_directories(${PROJECT_NAME} PUBLIC
    ${PROJECT_SOURCE_DIR}/source
    ${PROJECT_SOURCE_DIR}/source/include
    ${Vulkan_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${FTYPE_INCLUDE_DIRS}
    ${UTIL_PATH}
    ${IMGUI_PATH}
  )

target_link_directories(${PROJECT_NAME} PUBLIC
  ${Vulkan_LIBRARIES}
  ${SDL2_LIB}
  ${FTYPE_LIB}
)

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

  target_link_libraries(${PROJECT_NAME} SDL2 SDL2_image freetype vulkan-1)
  
  message(STATUS "COPYING DLLs")
  file(COPY ${SDL2_DLL} DESTINATION "./Release")

elseif (UNIX)
    message(STATUS "CREATING BUILD FOR UNIX")

    if (APPLE)
      target_link_libraries(${PROJECT_NAME} libSDL2-2.0.0.dylib libSDL2_image-2.0.0.dylib libfreetype.6.dylib libvulkan.1.3.236.dylib)
    endif()

endif()