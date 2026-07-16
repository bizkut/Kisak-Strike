if(NOT KISAK_PS4_NM OR NOT EXISTS "${KISAK_PS4_NM}")
    message(FATAL_ERROR "KISAK_PS4_NM must name the archive-compatible nm tool")
endif()
if(NOT KISAK_PS4_BINARY OR NOT EXISTS "${KISAK_PS4_BINARY}")
    message(FATAL_ERROR "KISAK_PS4_BINARY must name the linked monolithic ELF")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/ps4_monolithic_retention.cmake")
execute_process(
    COMMAND "${KISAK_PS4_NM}" --defined-only --format=posix "${KISAK_PS4_BINARY}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE defined_symbols
    ERROR_VARIABLE nm_error)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Failed to inspect ${KISAK_PS4_BINARY}: ${nm_error}")
endif()

foreach(symbol IN LISTS
        KISAK_PS4_CLIENT_RETENTION_SYMBOLS
        KISAK_PS4_ENGINE_RETENTION_SYMBOLS
        KISAK_PS4_SERVER_RETENTION_SYMBOLS
        KISAK_PS4_VPHYSICS_RETENTION_SYMBOLS)
    if(NOT defined_symbols MATCHES "(^|\n)${symbol} [A-Za-z]")
        message(FATAL_ERROR
            "PS4 monolithic retention symbol is missing after link: ${symbol}")
    endif()
endforeach()
