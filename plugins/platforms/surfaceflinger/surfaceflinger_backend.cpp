/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2015 Martin Gräßlin <mgraesslin@kde.org>
Copyright (C) 2016 Michael Serpieri <mickybart@pygoscelis.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "egl_surfaceflinger_backend.h"
#include "surfaceflinger_backend.h"
#include "logging.h"
#include "screens_surfaceflinger.h"
#include "composite.h"
#include "input.h"
#include "wayland_server.h"
#include "surfaceflinger_screeninfo.h"
// KWayland
#include <KWayland/Server/display.h>
#include <KWayland/Server/output_interface.h>
#include <KWayland/Server/seat_interface.h>
// Qt
#include <QKeyEvent>
// hybris/android
#include <hardware/hardware.h>
#include <hardware/lights.h>
#include <hybris_nativebufferext.h>
// linux
#include <linux/input.h>

// based on test_hwcomposer.c/test_sf.c from libhybris project (Apache 2 licensed)
// and qt5-qpa-surfaceflinger-plugin

namespace KWin
{

BacklightInputEventFilter::BacklightInputEventFilter(SurfaceFlingerBackend *backend)
    : InputEventFilter()
    , m_backend(backend)
{
}

BacklightInputEventFilter::~BacklightInputEventFilter() = default;

bool BacklightInputEventFilter::pointerEvent(QMouseEvent *event, quint32 nativeButton)
{
    Q_UNUSED(event)
    Q_UNUSED(nativeButton)
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    toggleBacklight();
    return true;
}

bool BacklightInputEventFilter::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event)
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    toggleBacklight();
    return true;
}

bool BacklightInputEventFilter::keyEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_PowerOff && event->type() == QEvent::KeyRelease) {
        toggleBacklight();
        return true;
    }
    return m_backend->isBacklightOff();
}

bool BacklightInputEventFilter::touchDown(quint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(pos)
    Q_UNUSED(time)
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    if (m_touchPoints.isEmpty()) {
        if (!m_doubleTapTimer.isValid()) {
            // this is the first tap
            m_doubleTapTimer.start();
        } else {
            if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
                m_secondTap = true;
            } else {
                // took too long. Let's consider it a new click
                m_doubleTapTimer.restart();
            }
        }
    } else {
        // not a double tap
        m_doubleTapTimer.invalidate();
        m_secondTap = false;
    }
    m_touchPoints << id;
    return true;
}

bool BacklightInputEventFilter::touchUp(quint32 id, quint32 time)
{
    Q_UNUSED(time)
    m_touchPoints.removeAll(id);
    if (!m_backend->isBacklightOff()) {
        return false;
    }
    if (m_touchPoints.isEmpty() && m_doubleTapTimer.isValid() && m_secondTap) {
        if (m_doubleTapTimer.elapsed() < qApp->doubleClickInterval()) {
            toggleBacklight();
        }
        m_doubleTapTimer.invalidate();
        m_secondTap = false;
    }
    return true;
}

bool BacklightInputEventFilter::touchMotion(quint32 id, const QPointF &pos, quint32 time)
{
    Q_UNUSED(id)
    Q_UNUSED(pos)
    Q_UNUSED(time)
    return m_backend->isBacklightOff();
}

void BacklightInputEventFilter::toggleBacklight()
{
    // queued to not modify the list of event filters while filtering
    QMetaObject::invokeMethod(m_backend, "toggleBlankOutput", Qt::QueuedConnection);
}

SurfaceFlingerBackend::SurfaceFlingerBackend(QObject *parent)
    : Platform(parent)
{
    handleOutputs();
}

SurfaceFlingerBackend::~SurfaceFlingerBackend()
{
    if (!m_outputBlank) {
        toggleBlankOutput();
    }
    
    //TODO: cleanup handle for m_sf_client and m_sf_surface
}

static KWayland::Server::OutputInterface *createOutput(size_t display_id)
{
    //get screen info
    SurfaceFlingerScreenInfo screen_info(display_id);

    using namespace KWayland::Server;
    OutputInterface *o = waylandServer()->display()->createOutput(waylandServer()->display());
    o->addMode(screen_info.screenSize(), OutputInterface::ModeFlag::Current | OutputInterface::ModeFlag::Preferred, screen_info.refreshRate() * 1000.0f);

    o->setPhysicalSize(screen_info.physicalScreenSize().toSize());
    
    o->create();
    return o;
}

void SurfaceFlingerBackend::init()
{
    m_sf_client = sf_client_create();
    SF_PLUGIN_ASSERT_NOT_NULL(m_sf_client);

    initLights();
    toggleBlankOutput();
    m_filter.reset(new BacklightInputEventFilter(this));
    input()->prepandInputEventFilter(m_filter.data());

    // get display configuration
    // TODO: support multiple screen (main display is 0, hdmi is 1)
    auto output = createOutput(0);
    if (!output) {
        emit initFailed();
        return;
    }
    m_displaySize = output->pixelSize();
    m_refreshRate = output->refreshRate();
    if (m_lights) {
        using namespace KWayland::Server;
        output->setDpmsSupported(true);
        auto updateDpms = [this, output] {
            output->setDpmsMode(m_outputBlank ? OutputInterface::DpmsMode::Off : OutputInterface::DpmsMode::On);
        };
        updateDpms();
        connect(this, &SurfaceFlingerBackend::outputBlankChanged, this, updateDpms);
        connect(output, &OutputInterface::dpmsModeRequested, this,
            [this] (KWayland::Server::OutputInterface::DpmsMode mode) {
                if (mode == OutputInterface::DpmsMode::On) {
                    if (m_outputBlank) {
                        toggleBlankOutput();
                    }
                } else {
                    if (!m_outputBlank) {
                        toggleBlankOutput();
                    }
                }
            }
        );
    }
    qCDebug(KWIN_SURFACEFLINGER) << "Display size:" << m_displaySize;
    qCDebug(KWIN_SURFACEFLINGER) << "Refresh rate:" << m_refreshRate;

    emit screensQueried();
    setReady(true);
}

void SurfaceFlingerBackend::initLights()
{
    hw_module_t *lightsModule = nullptr;
    if (hw_get_module(LIGHTS_HARDWARE_MODULE_ID, (const hw_module_t **)&lightsModule) != 0) {
        qCWarning(KWIN_SURFACEFLINGER) << "Failed to get lights module";
        return;
    }
    light_device_t *lightsDevice = nullptr;
    if (lightsModule->methods->open(lightsModule, LIGHT_ID_BACKLIGHT, (hw_device_t **)&lightsDevice) != 0) {
        qCWarning(KWIN_SURFACEFLINGER) << "Failed to create lights device";
        return;
    }
    m_lights = lightsDevice;
}

void SurfaceFlingerBackend::toggleBlankOutput()
{
    if (!m_sf_client) {
        return;
    }
    m_outputBlank = !m_outputBlank;
    toggleScreenBrightness();
    
    if (m_outputBlank) {
        sf_blank(0);
    } else {
        sf_unblank(0);
    }

    // enable/disable compositor repainting when blanked
    setOutputsEnabled(!m_outputBlank);
    if (Compositor *compositor = Compositor::self()) {
        if (!m_outputBlank) {
            compositor->addRepaintFull();
        }
    }
    emit outputBlankChanged();
}

void SurfaceFlingerBackend::toggleScreenBrightness()
{
    if (!m_lights) {
        return;
    }
    const int brightness = m_outputBlank ? 0 : 0xFF;
    struct light_state_t state;
    state.flashMode = LIGHT_FLASH_NONE;
    state.brightnessMode = BRIGHTNESS_MODE_USER;

    state.color = (int)((0xffU << 24) | (brightness << 16) |
                        (brightness << 8) | brightness);
    m_lights->set_light(m_lights, &state);
}

EGLDisplay SurfaceFlingerBackend::display()
{
    //EGL_DEFAULT_DISPLAY
    return sf_client_get_egl_display(m_sf_client);
}

EGLSurface SurfaceFlingerBackend::createEglSurface()
{
    qCDebug(KWIN_SURFACEFLINGER) << "CreateSurface";
    
    SfSurfaceCreationParameters params = {
            0,
            0,
            size().width(),
            size().height(),
            HYBRIS_PIXEL_FORMAT_RGBA_8888, //PIXEL_FORMAT_RGBA_8888
            INT_MAX, //layer position
            1.0f, //opacity
            1, //create_egl_window_surface
            "kwin-backend"
    };

    m_sf_surface = sf_surface_create(m_sf_client, &params);
    SF_PLUGIN_ASSERT_NOT_NULL(m_sf_surface);
    
    qCDebug(KWIN_SURFACEFLINGER) << "Surface:" << m_sf_surface;
    
    return sf_surface_get_egl_surface(m_sf_surface);
}

void SurfaceFlingerBackend::makeCurrent()
{
    sf_surface_make_current(m_sf_surface);
}

EGLConfig SurfaceFlingerBackend::config()
{
    return sf_client_get_egl_config(m_sf_client);
}

EGLContext SurfaceFlingerBackend::context()
{
    return sf_client_get_egl_context(m_sf_client);
}

Screens *SurfaceFlingerBackend::createScreens(QObject *parent)
{
    return new SurfaceFlingerScreens(this, parent);
}

OpenGLBackend *SurfaceFlingerBackend::createOpenGLBackend()
{
    return new EglSurfaceFlingerBackend(this);
}

}
