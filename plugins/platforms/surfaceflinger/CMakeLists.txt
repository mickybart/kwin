set(SURFACEFLINGER_SOURCES
    egl_surfaceflinger_backend.cpp
    logging.cpp
    screens_surfaceflinger.cpp
    surfaceflinger_backend.cpp
    surfaceflinger_screeninfo.cpp
)

add_library(KWinWaylandSurfaceFlingerBackend MODULE ${SURFACEFLINGER_SOURCES})
target_link_libraries(KWinWaylandSurfaceFlingerBackend
    kwin
    libhybris::libhardware
    libhybris::libsf
    libhybris::hybriseglplatform
)

install(
    TARGETS
        KWinWaylandSurfaceFlingerBackend
    DESTINATION
        ${PLUGIN_INSTALL_DIR}/org.kde.kwin.waylandbackends/
)
