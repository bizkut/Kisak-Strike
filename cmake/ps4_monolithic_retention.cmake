# Static-module providers whose translation units can otherwise remain dormant
# inside an archive because their only consumers are static constructors.
# Keep the lists separated by Source ownership so duplicate providers stay
# visible during monolithic-composition review.
set(KISAK_PS4_CLIENT_RETENTION_SYMBOLS
    cl_disable_water_render_targets
    _Z23ProcessRenderToRTHelperv
    g_CVGuiScreenPanelFactory
    g_CMovieDisplayScreenFactory
    g_CSlideshowDisplayScreenFactory
    g_CC4PanelFactory
    g_CViewC4PanelFactory
)

set(KISAK_PS4_ENGINE_RETENTION_SYMBOLS)
set(KISAK_PS4_SERVER_RETENTION_SYMBOLS)
set(KISAK_PS4_VPHYSICS_RETENTION_SYMBOLS
    g_SurfaceDatabase
)

function(kisak_ps4_apply_monolithic_retention target)
    foreach(symbol IN LISTS
            KISAK_PS4_CLIENT_RETENTION_SYMBOLS
            KISAK_PS4_ENGINE_RETENTION_SYMBOLS
            KISAK_PS4_SERVER_RETENTION_SYMBOLS
            KISAK_PS4_VPHYSICS_RETENTION_SYMBOLS)
        target_link_options(${target} PRIVATE "--undefined=${symbol}")
    endforeach()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}"
            "-DKISAK_PS4_NM=${CMAKE_NM}"
            "-DKISAK_PS4_BINARY=$<TARGET_FILE:${target}>"
            -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/verify_ps4_retained_symbols.cmake"
        COMMENT "Verifying PS4 monolithic retention manifest"
        VERBATIM)
endfunction()
