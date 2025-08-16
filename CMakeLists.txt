cmake_minimum_required(VERSION 3.24)
project(CampusMap LANGUAGES CXX)

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Удобно для IDE/анализа
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(campus_map
    src/main.cpp "src/Map_Visuality/Map_Viewer.cpp" "src/Engine/Engine.cpp" "src/Camera/Camera.cpp" "src/map/Graph.cpp")

# Опции (можно быстро выключать доп. либы)
option(USE_SDL_IMAGE "Use SDL2_image (PNG, JPG, etc.)" ON)
option(USE_SDL_TTF   "Use SDL2_ttf (fonts)"            ON)

# Варнинги
if (MSVC)
  target_compile_options(campus_map PRIVATE /W4 /permissive-)
else()
  target_compile_options(campus_map PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Если конфигурируем через emcmake / Emscripten toolchainейч
if (EMSCRIPTEN)
  message(STATUS "Target: Web (Emscripten)")

  # Именование вывода: index.html удобно для GitHub Pages
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  set_target_properties(campus_map PROPERTIES OUTPUT_NAME "index")

  # Базовые флаги
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

  # Больше проверок в Debug
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND EM_FLAGS "-sASSERTIONS=2" "-sSAFE_HEAP=1")
  endif()

  # Если есть папка assets — предзагрузим её в виртуальную FS по пути /assets
  if (EXISTS "${CMAKE_SOURCE_DIR}/assets")
    target_link_options(campus_map PRIVATE
      "--preload-file" "${CMAKE_SOURCE_DIR}/assets@/assets"
    )
  endif()

  # Применим флаги и на компиляцию, и на линковку
  target_compile_options(campus_map PRIVATE ${EM_FLAGS})
  target_link_options(campus_map    PRIVATE ${EM_FLAGS})

  # Опционально: свой HTML-шаблон, если есть web/shell.html
  if (EXISTS "${CMAKE_SOURCE_DIR}/web/shell.html")
    target_link_options(campus_map PRIVATE
      "--shell-file" "${CMAKE_SOURCE_DIR}/web/shell.html"
    )
  endif()
endif()

if (NOT EMSCRIPTEN)
    message(STATUS "Target: Native (Windows)")

    find_package(SDL2 CONFIG REQUIRED)
    find_package(SDL2_image CONFIG REQUIRED)
    find_package(SDL2_ttf CONFIG REQUIRED)


    target_link_libraries(campus_map PRIVATE
        SDL2::SDL2
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
    )
endif()

if (EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(
      nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
    target_link_libraries(campus_map PRIVATE nlohmann_json::nlohmann_json)
else()
    find_package(nlohmann_json CONFIG REQUIRED)
    target_link_libraries(campus_map PRIVATE nlohmann_json::nlohmann_json)
endif()