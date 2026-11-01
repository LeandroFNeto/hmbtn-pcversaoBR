# Compiler launcher of the host target (cmake/host.cmake):
#   cmake -P host_cc_check.cmake -- <compiler> <arguments>...
# Runs the compiler and fails the compile when GCC dropped the static initializers of a
# zero-length array, which GCC 2.95 kept after the struct. The fix is a host patch
# (hmbtn_host_patch_header in cmake/host.cmake), never an edit under src/.

set(command "")
set(object "")
set(started FALSE)
set(next_is_object FALSE)
math(EXPR last "${CMAKE_ARGC} - 1")
foreach(i RANGE ${last})
    set(arg "${CMAKE_ARGV${i}}")
    if(started)
        list(APPEND command "${arg}")
        if(next_is_object)
            set(object "${arg}")
            set(next_is_object FALSE)
        elseif(arg STREQUAL "-o")
            set(next_is_object TRUE)
        endif()
    elseif(arg STREQUAL "--")
        set(started TRUE)
    endif()
endforeach()
if(NOT command)
    message(FATAL_ERROR "host_cc_check: no compiler command after --")
endif()

execute_process(COMMAND ${command}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out ERROR_VARIABLE err
    ECHO_OUTPUT_VARIABLE ECHO_ERROR_VARIABLE)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "host_cc_check: the compiler exited with ${result}")
endif()

string(REGEX MATCHALL "[^\n]*excess elements in array initializer[^\n]*" dropped "${out}${err}")
if(dropped)
    # Without the object, the next build compiles the file again instead of skipping it.
    if(object)
        file(REMOVE "${object}")
    endif()
    set(places "")
    foreach(warning IN LISTS dropped)
        string(REGEX REPLACE "^(.*):([0-9]+):[0-9]+: .*$" "  \\1:\\2" place "${warning}")
        list(APPEND places "${place}")
    endforeach()
    list(REMOVE_DUPLICATES places)
    list(JOIN places "\n" lines)
    message(FATAL_ERROR
        "host_cc_check: GCC dropped array initializers that GCC 2.95 kept; patch the "
        "declaration with hmbtn_host_patch_header in cmake/host.cmake:\n${lines}")
endif()
