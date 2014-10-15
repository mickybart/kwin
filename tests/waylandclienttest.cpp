/*
 * Copyright 2014  Martin Gräßlin <mgraesslin@kde.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "waylandclienttest.h"
// KWin::Wayland
#include <KWayland/Client/buffer.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
// Qt
#include <QAbstractEventDispatcher>
#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QThread>
#include <QTimer>
// Wayland
#include <wayland-client-protocol.h>

#include <linux/input.h>

using namespace KWayland::Client;

static Qt::GlobalColor s_colors[] = {
    Qt::white,
    Qt::red,
    Qt::green,
    Qt::blue,
    Qt::black
};
static int s_colorIndex = 0;

WaylandClientTest::WaylandClientTest(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread(nullptr))
    , m_eventQueue(nullptr)
    , m_compositor(new Compositor(this))
    , m_output(new Output(this))
    , m_surface(nullptr)
    , m_shm(new ShmPool(this))
    , m_timer(new QTimer(this))
{
    init();
}

WaylandClientTest::~WaylandClientTest()
{
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
}

void WaylandClientTest::init()
{
    connect(m_connectionThreadObject, &ConnectionThread::connected, this,
        [this]() {
            // create the event queue for the main gui thread
            wl_display *display = m_connectionThreadObject->display();
            m_eventQueue = wl_display_create_queue(display);
            // setup registry
            Registry *registry = new Registry(this);
            setupRegistry(registry);

            QAbstractEventDispatcher *dispatcher = QCoreApplication::instance()->eventDispatcher();
            connect(dispatcher, &QAbstractEventDispatcher::aboutToBlock, this,
                [this]() {
                    wl_display_flush(m_connectionThreadObject->display());
                }
            );
        },
        Qt::QueuedConnection);
    connect(m_connectionThreadObject, &ConnectionThread::eventsRead, this,
        [this]() {
            if (!m_eventQueue) {
                return;
            }
            wl_display_dispatch_queue_pending(m_connectionThreadObject->display(), m_eventQueue);
            wl_display_flush(m_connectionThreadObject->display());
        },
        Qt::QueuedConnection);

    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();

    m_connectionThreadObject->initConnection();

    connect(m_timer, &QTimer::timeout, this,
        [this]() {
            s_colorIndex = (s_colorIndex + 1) % 5;
            render();
        }
    );
    m_timer->setInterval(1000);
    m_timer->start();
}

void WaylandClientTest::setupRegistry(Registry *registry)
{
    connect(registry, &Registry::compositorAnnounced, this,
        [this, registry](quint32 name) {
            m_compositor->setup(registry->bindCompositor(name, 1));
            m_surface = m_compositor->createSurface(this);
        }
    );
    connect(registry, &Registry::shellAnnounced, this,
        [this, registry](quint32 name) {
            Shell *shell = new Shell(this);
            shell->setup(registry->bindShell(name, 1));
            ShellSurface *shellSurface = shell->createSurface(m_surface, m_surface);
            shellSurface->setFullscreen(m_output);
            connect(shellSurface, &ShellSurface::sizeChanged, this, static_cast<void(WaylandClientTest::*)(const QSize&)>(&WaylandClientTest::render));
        }
    );
    connect(registry, &Registry::outputAnnounced, this,
        [this, registry](quint32 name) {
            if (m_output->isValid()) {
                return;
            }
            m_output->setup(registry->bindOutput(name, 2));
        }
    );
    connect(registry, &Registry::shmAnnounced, this,
        [this, registry](quint32 name) {
            m_shm->setup(registry->bindShm(name, 1));
        }
    );
    connect(registry, &Registry::seatAnnounced, this,
        [this, registry](quint32 name) {
            Seat *s = new Seat(this);
            connect(s, &Seat::hasKeyboardChanged, this,
                [this, s](bool has) {
                    if (!has) {
                        return;
                    }
                    Keyboard *k = s->createKeyboard(this);
                    connect(k, &Keyboard::keyChanged, this,
                        [this](quint32 key, Keyboard::KeyState state) {
                            if (key == KEY_Q && state == Keyboard::KeyState::Released) {
                                QCoreApplication::instance()->quit();
                            }
                        }
                    );
                }
            );
            connect(s, &Seat::hasPointerChanged, this,
                [this, s](bool has) {
                    if (!has) {
                        return;
                    }
                    Pointer *p = s->createPointer(this);
                    connect(p, &Pointer::buttonStateChanged, this,
                        [this](quint32 serial, quint32 time, quint32 button, Pointer::ButtonState state) {
                            Q_UNUSED(serial)
                            Q_UNUSED(time)
                            if (state == Pointer::ButtonState::Released) {
                                if (button == BTN_LEFT) {
                                    if (m_timer->isActive()) {
                                        m_timer->stop();
                                    } else {
                                        m_timer->start();
                                    }
                                }
                                if (button == BTN_RIGHT) {
                                    QCoreApplication::instance()->quit();
                                }
                            }
                        }
                    );
                }
            );
            s->setup(registry->bindSeat(name, 2));
        }
    );
    registry->create(m_connectionThreadObject->display());
    wl_proxy_set_queue((wl_proxy*)registry->registry(), m_eventQueue);
    registry->setup();
}

void WaylandClientTest::render(const QSize &size)
{
    m_currentSize = size;
    render();
}

void WaylandClientTest::render()
{
    if (!m_shm || !m_surface || !m_surface->isValid() || !m_currentSize.isValid()) {
        return;
    }
    auto buffer = m_shm->getBuffer(m_currentSize, m_currentSize.width() * 4).toStrongRef();
    buffer->setUsed(true);
    QImage image(buffer->address(), m_currentSize.width(), m_currentSize.height(), QImage::Format_ARGB32_Premultiplied);
    image.fill(s_colors[s_colorIndex]);

    QPainter p;
    p.begin(&image);
    QImage icon(QStringLiteral(SOURCE_DIR) + QStringLiteral("/48-apps-kwin.png"));
    p.drawImage(QPoint(0, 0), icon);
    p.end();

    m_surface->attachBuffer(*buffer);
    m_surface->damage(QRect(QPoint(0, 0), m_currentSize));
    m_surface->commit(Surface::CommitFlag::None);
    buffer->setUsed(false);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    new WaylandClientTest(&app);

    return app.exec();
}