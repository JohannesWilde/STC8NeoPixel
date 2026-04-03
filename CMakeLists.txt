cmake_minimum_required(VERSION 3.16)
set(CMAKE_TOOLCHAIN_FILE "toolchain.cmake")
project(STC8NeoPixel LANGUAGES C)

function(ihx_to_hex bin)
    add_custom_command(TARGET ${bin} POST_BUILD COMMAND  packihx ${bin}.ihx > ${bin}.hex)
endfunction(ihx_to_hex)

add_compile_definitions(
      $<$<NOT:$<OR:$<CONFIG:RelWithDebInfo>,$<CONFIG:Debug>>>:NDEBUG>
)

add_subdirectory(src)
