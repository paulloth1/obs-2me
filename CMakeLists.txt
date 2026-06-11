cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
  # Diagnose: welche Include-Dir liefert der Target wirklich?
  if(TARGET OBS::obs-frontend-api)
    get_target_property(_fa_inc OBS::obs-frontend-api INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "2ME-DIAG: OBS::obs-frontend-api INTERFACE_INCLUDE_DIRECTORIES = ${_fa_inc}")
  endif()
  message(STATUS "2ME-DIAG: obs-frontend-api_DIR = ${obs-frontend-api_DIR}")

  # Robuste Fallback-Suche nach obs-frontend-api.h (Quelle ODER Install-Prefix)
  file(
    GLOB _fa_cands
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-studio-*/UI/obs-frontend-api/obs-frontend-api.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/include/obs-frontend-api.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/include/obs/obs-frontend-api.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/*/include/obs-frontend-api.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/*/include/obs/obs-frontend-api.h"
  )
  message(STATUS "2ME-DIAG: frontend-api Header-Kandidaten = ${_fa_cands}")
  if(_fa_cands)
    list(GET _fa_cands 0 _fa0)
    get_filename_component(_fadir "${_fa0}" DIRECTORY)
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${_fadir}")
    message(STATUS "2ME-DIAG: frontend-api Include ergänzt -> ${_fadir}")
  else()
    message(WARNING "2ME-DIAG: obs-frontend-api.h NICHT in .deps gefunden")
  endif()
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.c src/me-bank-source.c src/me-scenes.c src/me-websocket.c src/me-dock.cpp src/me-multiview.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
