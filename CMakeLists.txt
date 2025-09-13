cmake_minimum_required(VERSION 3.24)
project(CampusMap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(campus_map
    src/main.cpp
    src/Map_Visuality/Map_Viewer.cpp
    src/Engine/Engine.cpp
    src/Camera/Camera.cpp
    src/map/Graph.cpp
    src/json_loader/json_loader.cpp
    src/path_finder/path_finder.cpp
 "src/aliases/AliasManager.cpp")

option(USE_SDL_IMAGE "Use SDL2_image (PNG, JPG, etc.)" ON)
option(USE_SDL_TTF   "Use SDL2_ttf (fonts)"            ON)

if (MSVC)
  target_compile_options(campus_map PRIVATE /W4 /permissive-)
else()
  target_compile_options(campus_map PRIVATE -Wall -Wextra -Wpedantic)
endif()

if (EMSCRIPTEN)
  message(STATUS "Target: Web (Emscripten)")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set_target_properties(campus_map PROPERTIES OUTPUT_NAME "index")

  set(EM_FLAGS
      "-sUSE_SDL=2"
      "-sALLOW_MEMORY_GROWTH=1"
  )
  if (USE_SDL_IMAGE)
    list(APPEND EM_FLAGS "-sUSE_SDL_IMAGE=2" "-sSDL2_IMAGE_FORMATS=['png']")
  endif()
  if (USE_SDL_TTF)
    list(APPEND EM_FLAGS "-sUSE_SDL_TTF=2")
  endif()
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND EM_FLAGS "-sASSERTIONS=2" "-sSAFE_HEAP=1")
  endif()
  if (EXISTS "${CMAKE_SOURCE_DIR}/assets")
    set_target_properties(campus_map PROPERTIES SUFFIX ".html")
    set_target_properties(campus_map PROPERTIES OUTPUT_NAME "index")

    target_link_options(campus_map PRIVATE
    "--shell-file=${CMAKE_SOURCE_DIR}/web_shell.html"
    "--preload-file=${CMAKE_SOURCE_DIR}/assets@/assets"
    )
  endif()
  target_compile_options(campus_map PRIVATE ${EM_FLAGS})
  target_link_options(campus_map    PRIVATE ${EM_FLAGS})

  # nlohmann_json через FetchContent
  include(FetchContent)
  FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
  )
  FetchContent_MakeAvailable(nlohmann_json)
  target_link_libraries(campus_map PRIVATE nlohmann_json::nlohmann_json)

else()
  message(STATUS "Target: Native (Windows)")
  find_package(SDL2 CONFIG REQUIRED)
  if (USE_SDL_IMAGE)
    find_package(SDL2_image CONFIG REQUIRED)
    target_link_libraries(campus_map PRIVATE SDL2_image::SDL2_image)
  endif()
  if (USE_SDL_TTF)
    find_package(SDL2_ttf CONFIG REQUIRED)
    target_link_libraries(campus_map PRIVATE SDL2_ttf::SDL2_ttf)
  endif()
  target_link_libraries(campus_map PRIVATE SDL2::SDL2 SDL2::SDL2main)

  # nlohmann_json через vcpkg
  find_package(nlohmann_json CONFIG REQUIRED)
  target_link_libraries(campus_map PRIVATE nlohmann_json::nlohmann_json)
endif()