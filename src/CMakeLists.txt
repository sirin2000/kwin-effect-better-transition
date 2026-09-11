set(bettertransition_SOURCES
    main.cpp
    bettertransition.cpp
    bettertransition.h
)

kconfig_add_kcfg_files(bettertransition_SOURCES
    bettertransitionconfig.kcfgc
)

add_library(bettertransition MODULE ${bettertransition_SOURCES})

# Teach AUTOMOC that the KWin factory macros contain a Q_OBJECT class, so that
# main.cpp's "#include \"main.moc\"" is handled without a warning.
set_property(TARGET bettertransition APPEND PROPERTY AUTOMOC_MACRO_NAMES
    "KWIN_EFFECT_FACTORY"
    "KWIN_EFFECT_FACTORY_SUPPORTED"
    "KWIN_EFFECT_FACTORY_ENABLED"
    "KWIN_EFFECT_FACTORY_SUPPORTED_ENABLED"
)

target_link_libraries(bettertransition PRIVATE
    KF6::ConfigCore
    KF6::ConfigGui
    KWin::kwin
)

target_compile_options(bettertransition PRIVATE -Wall -Wextra)

install(TARGETS bettertransition
    DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins
)

add_subdirectory(kcm)
