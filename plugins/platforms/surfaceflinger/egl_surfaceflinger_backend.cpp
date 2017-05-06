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

namespace KWin
{

EglSurfaceFlingerBackend::EglSurfaceFlingerBackend(SurfaceFlingerBackend *backend)
    : AbstractEglBackend()
    , m_backend(backend)
{
    // EGL is always direct rendering
    setIsDirectRendering(true);
    setSyncsToVBlank(true);
    //setBlocksForRetrace(true);
}

EglSurfaceFlingerBackend::~EglSurfaceFlingerBackend()
{
    cleanup();
}

bool EglSurfaceFlingerBackend::initializeEgl()
{
    EGLDisplay display = m_backend->sceneEglDisplay();

    if (display == EGL_NO_DISPLAY) {
        display = m_backend->display();
    }
    if (display == EGL_NO_DISPLAY) {
	return false;
    }
    setEglDisplay(display);
    return initEglAPI();
}

void EglSurfaceFlingerBackend::init()
{
    qCDebug(KWIN_SURFACEFLINGER) << "EglSurfaceFlingerBackend::init";
    
    if (!initializeEgl()) {
        setFailed("Failed to initialize egl");
        return;
    }
    if (!initRenderingContext()) {
        setFailed("Could not initialize rendering context");
        return;
    }

    initKWinGL();
    initBufferAge();
    initWayland();
}

bool EglSurfaceFlingerBackend::initBufferConfigs()
{
    setConfig(m_backend->config());

    return true;
}

bool EglSurfaceFlingerBackend::initRenderingContext()
{
    if (!initBufferConfigs()) {
        return false;
    }

    setContext(m_backend->context());
    
    setSurface(m_backend->createEglSurface());

    return makeContextCurrent();
}

bool EglSurfaceFlingerBackend::makeContextCurrent()
{
    m_backend->makeCurrent();
    
    return true;
}

void EglSurfaceFlingerBackend::present()
{
    if (lastDamage().isEmpty()) {
        return;
    }

    eglSwapBuffers(eglDisplay(), surface());
    setLastDamage(QRegion());
}

void EglSurfaceFlingerBackend::screenGeometryChanged(const QSize &size)
{
    Q_UNUSED(size)
}

QRegion EglSurfaceFlingerBackend::prepareRenderingFrame()
{
    present();

    // TODO: buffer age?
    startRenderTimer();
    // triggers always a full repaint
    return QRegion(QRect(QPoint(0, 0), m_backend->size()));
}

void EglSurfaceFlingerBackend::endRenderingFrame(const QRegion &renderedRegion, const QRegion &damagedRegion)
{
    Q_UNUSED(damagedRegion)
    setLastDamage(renderedRegion);
}

SceneOpenGL::TexturePrivate *EglSurfaceFlingerBackend::createBackendTexture(SceneOpenGL::Texture *texture)
{
    return new EglSurfaceFlingerTexture(texture, this);
}

bool EglSurfaceFlingerBackend::usesOverlayWindow() const
{
    return false;
}

EglSurfaceFlingerTexture::EglSurfaceFlingerTexture(SceneOpenGL::Texture *texture, EglSurfaceFlingerBackend *backend)
    : AbstractEglTexture(texture, backend)
{
}

EglSurfaceFlingerTexture::~EglSurfaceFlingerTexture() = default;

}
