cmake_minimum_required(VERSION 3.16)
project(mathjax-c VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE REQUIRED freetype2)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)

add_library(mathjax STATIC
  src/api.c
  src/ast/node.c
  src/ast/factory.c
  src/ast/visitor.c
  src/font/font.c
  src/font/metrics.c
  src/font/glyph.c
  src/render/bitmap.c
  src/render/draw.c
  src/kitty/kitty.c
  src/layout/box.c
  src/layout/math.c
  src/layout/scripts.c
  src/layout/fractions.c
  src/layout/radicals.c
  src/layout/matrix.c
  src/layout/spacing.c
  src/parser/lexer.c
  src/parser/parser.c
  src/parser/macros.c
  src/parser/symbols.c
  src/parser/environments.c
)

target_include_directories(mathjax PUBLIC
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/src/third_party
  ${FREETYPE_INCLUDE_DIRS}
  ${HARFBUZZ_INCLUDE_DIRS}
)

target_link_directories(mathjax PUBLIC
  ${FREETYPE_LIBRARY_DIRS}
  ${HARFBUZZ_LIBRARY_DIRS}
)
target_link_libraries(mathjax PUBLIC
  ${FREETYPE_LIBRARIES}
  ${HARFBUZZ_LIBRARIES}
  m
)

if(APPLE)
  find_library(CORE_TEXT CoreText)
  find_library(CORE_GRAPHICS CoreGraphics)
  if(CORE_TEXT AND CORE_GRAPHICS)
    target_link_libraries(mathjax PRIVATE ${CORE_TEXT} ${CORE_GRAPHICS})
  endif()
endif()

add_executable(mathjax-cli tools/mathjax-cli.c)
target_link_libraries(mathjax-cli PRIVATE mathjax)
