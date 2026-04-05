
add_library(WS2812
    ws2812.h
    ws2812.c
)

add_executable(${PROJECT_NAME}
    8051_helpers.h
    main.c
    stc8g.h
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE WS2812
)

ihx_to_hex(${PROJECT_NAME})

include_directories("c:/Program Files/SDCC/include/")

# C:\Users\User\AppData\Local\Python\pythoncore-3.14-64\python.exe -m stcgal --port COM8 --baud 115200 --protocol stc8g "C:\Users\User\Documents\STC\build\UsbTurnoffTimer-SDCC-Release\src\UsbTurnoffTimer.hex"

find_package(Python
    COMPONENTS Interpreter
)

if (NOT Python_Interpreter_FOUND)
    message(WARNING "No Python interpreter found - programming via stcgal won't be possible.\n"
                    "If Python is installed in an unusual place, please set the CMake variable Python_ROOT_DIR accordingly.")
else ()
    message(STATUS "Python found: ${Python_EXECUTABLE}")

    # Check whether stcgal is installed in the found Python interpreter using pip.
    # Todo: should I use "python -c "import sys;exec(\"try:\n  import stcgal\n  result = 0\nexcept:\n  result = -1\"); exit(result)'"
    #       and then check result to avoid dependency on pip?
    execute_process(COMMAND ${Python_EXECUTABLE} -m pip list
                    TIMEOUT 5
                    RESULT_VARIABLE result
                    OUTPUT_VARIABLE output
                    ERROR_VARIABLE error
    )

    if (NOT (0 EQUAL result))
        message(WARNING "Failed to query list of installed Python packeges using pip [result: ${result}, error: ${error}].")
    else ()
        set(STCGAL_PACKAGE_NAME "stcgal")
        if (NOT ("${output}" MATCHES "\n${STCGAL_PACKAGE_NAME}[ ]*[0-9\.]+\n"))
            message(WARNING "Required package \"${STCGAL_PACKAGE_NAME}\" not found.")
            message("pip list:\n${output}")
        else ()
            message(STATUS "Required package ${STCGAL_PACKAGE_NAME} found.")

            set(PROGRAMMER_BAUD_RATE "115200" CACHE STRING "Baud rate to be used by the programmer.")
            set(PROGRAMMER_PROTOCOL "stc8g" CACHE STRING "stcgal's protocol to use - it has to match the used device.")
            set(PROGRAMMER_COM_PORT "None" CACHE STRING "Name of the COM port used to communicate with the programmer.")

            add_custom_target(program${PROJECT_NAME}
                              ${Python_EXECUTABLE} -m stcgal
                                --port ${PROGRAMMER_COM_PORT}
                                --baud ${PROGRAMMER_BAUD_RATE}
                                --protocol ${PROGRAMMER_PROTOCOL} $<TARGET_FILE:${PROJECT_NAME}>
                              DEPENDS ${PROJECT_NAME}
                              COMMENT "Going to program $<TARGET_FILE:${PROJECT_NAME}> via ${PROGRAMMER_COM_PORT}."
            )

        endif ()
        unset(STCGAL_PACKAGE_NAME)
    endif ()
endif ()
