cmake_minimum_required(VERSION 3.24)
project(CampusMap LANGUAGES CXX)

# Стандарт C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Удобно для IDE/анализа
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Опции (можно быстро выключать доп. либы)
option(USE_SDL_IMAGE "Use SDL2_image (PNG, JPG, etc.)" ON)
option(USE_SDL_TTF   "Use SDL2_ttf (fonts)"            ON)

# Источники
add_executable(campus_map
    src/main.cpp
)

# Источник алгоритма поиска пути
add_executable(pathfinder
    src/path_finder/path_finder.cpp
)

# Источник алгоритма сбора информации о точках
add_executable(readnodes
    src/read_nodes/nodes.cpp
)

# Варнинги
if (MSVC)
  target_compile_options(campus_map PRIVATE /W4 /permissive-)
else()
  target_compile_options(campus_map PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Если конфигурируем через emcmake / Emscripten toolchain
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

else()
  message(STATUS "Target: Native (Windows)")

  # SDL2 от vcpkg (перед конфигурацией передай -DCMAKE_TOOLCHAIN_FILE=...vcpkg.cmake)
  find_package(SDL2 CONFIG REQUIRED)
  target_link_libraries(campus_map PRIVATE SDL2::SDL2 SDL2::SDL2main)

  if (USE_SDL_IMAGE)
    find_package(SDL2_image CONFIG REQUIRED)
    if (TARGET SDL2::SDL2_image)
      target_link_libraries(campus_map PRIVATE SDL2::SDL2_image)
    endif()
  endif()

  if (USE_SDL_TTF)
    find_package(SDL2_ttf CONFIG REQUIRED)
    if (TARGET SDL2::SDL2_ttf)
      target_link_libraries(campus_map PRIVATE SDL2::SDL2_ttf)
    endif()
  endif()
endif()