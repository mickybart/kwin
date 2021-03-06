/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "abstract_backend.h"
#include "cursor.h"
#include "shell_client.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"

#include <KWayland/Client/compositor.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>
#include <KWayland/Client/touch.h>

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_touch_input-0");

class TouchInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testMultipleTouchPoints();
    void testCancel();
    void testTouchMouseAction();

private:
    AbstractClient *showWindow();
    KWayland::Client::ConnectionThread *m_connection = nullptr;
    KWayland::Client::Compositor *m_compositor = nullptr;
    KWayland::Client::Seat *m_seat = nullptr;
    KWayland::Client::ShmPool *m_shm = nullptr;
    KWayland::Client::Shell *m_shell = nullptr;
    KWayland::Client::Touch *m_touch = nullptr;
    KWayland::Client::EventQueue *m_queue = nullptr;
    QThread *m_thread = nullptr;
};

void TouchInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    waylandServer()->backend()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(waylandServer()->backend(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    waylandServer()->init(s_socketName.toLocal8Bit());

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    waylandServer()->initWorkspace();
}

void TouchInputTest::init()
{
    using namespace KWayland::Client;
    // setup connection
    m_connection = new ConnectionThread;
    QSignalSpy connectedSpy(m_connection, &ConnectionThread::connected);
    QVERIFY(connectedSpy.isValid());
    m_connection->setSocketName(s_socketName);

    m_thread = new QThread(this);
    m_connection->moveToThread(m_thread);
    m_thread->start();

    m_connection->initConnection();
    QVERIFY(connectedSpy.wait());

    m_queue = new EventQueue(this);
    QVERIFY(!m_queue->isValid());
    m_queue->setup(m_connection);
    QVERIFY(m_queue->isValid());

    Registry registry;
    registry.setEventQueue(m_queue);
    QSignalSpy compositorSpy(&registry, &Registry::compositorAnnounced);
    QSignalSpy shmSpy(&registry, &Registry::shmAnnounced);
    QSignalSpy shellSpy(&registry, &Registry::shellAnnounced);
    QSignalSpy seatSpy(&registry, &Registry::seatAnnounced);
    QSignalSpy allAnnounced(&registry, &Registry::interfacesAnnounced);
    QVERIFY(allAnnounced.isValid());
    QVERIFY(shmSpy.isValid());
    QVERIFY(shellSpy.isValid());
    QVERIFY(compositorSpy.isValid());
    QVERIFY(seatSpy.isValid());
    registry.create(m_connection->display());
    QVERIFY(registry.isValid());
    registry.setup();
    QVERIFY(allAnnounced.wait());
    QVERIFY(!compositorSpy.isEmpty());
    QVERIFY(!shmSpy.isEmpty());
    QVERIFY(!shellSpy.isEmpty());
    QVERIFY(!seatSpy.isEmpty());

    m_compositor = registry.createCompositor(compositorSpy.first().first().value<quint32>(), compositorSpy.first().last().value<quint32>(), this);
    QVERIFY(m_compositor->isValid());
    m_shm = registry.createShmPool(shmSpy.first().first().value<quint32>(), shmSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shm->isValid());
    m_shell = registry.createShell(shellSpy.first().first().value<quint32>(), shellSpy.first().last().value<quint32>(), this);
    QVERIFY(m_shell->isValid());
    m_seat = registry.createSeat(seatSpy.first().first().value<quint32>(), seatSpy.first().last().value<quint32>(), this);
    QVERIFY(m_seat->isValid());
    QSignalSpy hasTouchSpy(m_seat, &Seat::hasTouchChanged);
    QVERIFY(hasTouchSpy.isValid());
    QVERIFY(hasTouchSpy.wait());
    m_touch = m_seat->createTouch(m_seat);
    QVERIFY(m_touch);
    QVERIFY(m_touch->isValid());

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(1280, 512));
}

void TouchInputTest::cleanup()
{
    delete m_compositor;
    m_compositor = nullptr;
    delete m_touch;
    m_touch = nullptr;
    delete m_seat;
    m_seat = nullptr;
    delete m_shm;
    m_shm = nullptr;
    delete m_shell;
    m_shell = nullptr;
    delete m_queue;
    m_queue = nullptr;
    if (m_thread) {
        m_connection->deleteLater();
        m_thread->quit();
        m_thread->wait();
        delete m_thread;
        m_thread = nullptr;
        m_connection = nullptr;
    }
}

AbstractClient *TouchInputTest::showWindow()
{
    using namespace KWayland::Client;
#define VERIFY(statement) \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return nullptr;
#define COMPARE(actual, expected) \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return nullptr;
    QSignalSpy clientAddedSpy(waylandServer(), &WaylandServer::shellClientAdded);
    VERIFY(clientAddedSpy.isValid());

    Surface *surface = m_compositor->createSurface(m_compositor);
    VERIFY(surface);
    ShellSurface *shellSurface = m_shell->createSurface(surface, surface);
    VERIFY(shellSurface);
    // let's render
    QImage img(QSize(100, 50), QImage::Format_ARGB32);
    img.fill(Qt::blue);
    surface->attachBuffer(m_shm->createBuffer(img));
    surface->damage(QRect(0, 0, 100, 50));
    surface->commit(Surface::CommitFlag::None);

    m_connection->flush();
    VERIFY(clientAddedSpy.wait());
    AbstractClient *c = workspace()->activeClient();
    VERIFY(c);
    COMPARE(clientAddedSpy.first().first().value<ShellClient*>(), c);

#undef VERIFY
#undef COMPARE

    return c;
}

void TouchInputTest::testMultipleTouchPoints()
{
    using namespace KWayland::Client;
    AbstractClient *c = showWindow();
    c->move(100, 100);
    QVERIFY(c);
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy pointAddedSpy(m_touch, &Touch::pointAdded);
    QVERIFY(pointAddedSpy.isValid());
    QSignalSpy pointMovedSpy(m_touch, &Touch::pointMoved);
    QVERIFY(pointMovedSpy.isValid());
    QSignalSpy pointRemovedSpy(m_touch, &Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());
    QSignalSpy endedSpy(m_touch, &Touch::sequenceEnded);
    QVERIFY(endedSpy.isValid());

    quint32 timestamp = 1;
    waylandServer()->backend()->touchDown(1, QPointF(125, 125), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 1);
    QCOMPARE(m_touch->sequence().first()->isDown(), true);
    QCOMPARE(m_touch->sequence().first()->position(), QPointF(25, 25));
    QCOMPARE(pointAddedSpy.count(), 0);
    QCOMPARE(pointMovedSpy.count(), 0);

    // a point outside the window
    waylandServer()->backend()->touchDown(2, QPointF(0, 0), timestamp++);
    QVERIFY(pointAddedSpy.wait());
    QCOMPARE(pointAddedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), true);
    QCOMPARE(m_touch->sequence().at(1)->position(), QPointF(-100, -100));
    QCOMPARE(pointMovedSpy.count(), 0);

    // let's move that one
    waylandServer()->backend()->touchMotion(2, QPointF(100, 100), timestamp++);
    QVERIFY(pointMovedSpy.wait());
    QCOMPARE(pointMovedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), true);
    QCOMPARE(m_touch->sequence().at(1)->position(), QPointF(0, 0));

    waylandServer()->backend()->touchUp(1, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 1);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().first()->isDown(), false);
    QCOMPARE(endedSpy.count(), 0);

    waylandServer()->backend()->touchUp(2, timestamp++);
    QVERIFY(pointRemovedSpy.wait());
    QCOMPARE(pointRemovedSpy.count(), 2);
    QCOMPARE(m_touch->sequence().count(), 2);
    QCOMPARE(m_touch->sequence().first()->isDown(), false);
    QCOMPARE(m_touch->sequence().at(1)->isDown(), false);
    QCOMPARE(endedSpy.count(), 1);
}

void TouchInputTest::testCancel()
{
    using namespace KWayland::Client;
    AbstractClient *c = showWindow();
    c->move(100, 100);
    QVERIFY(c);
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());
    QSignalSpy cancelSpy(m_touch, &Touch::sequenceCanceled);
    QVERIFY(cancelSpy.isValid());
    QSignalSpy pointRemovedSpy(m_touch, &Touch::pointRemoved);
    QVERIFY(pointRemovedSpy.isValid());

    quint32 timestamp = 1;
    waylandServer()->backend()->touchDown(1, QPointF(125, 125), timestamp++);
    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cancel
    waylandServer()->backend()->touchCancel();
    QVERIFY(cancelSpy.wait());
    QCOMPARE(cancelSpy.count(), 1);

    waylandServer()->backend()->touchUp(1, timestamp++);
    QVERIFY(!pointRemovedSpy.wait(100));
    QCOMPARE(pointRemovedSpy.count(), 0);
}

void TouchInputTest::testTouchMouseAction()
{
    // this test verifies that a touch down on an inactive client will activate it
    using namespace KWayland::Client;
    // create two windows
    AbstractClient *c1 = showWindow();
    QVERIFY(c1);
    AbstractClient *c2 = showWindow();
    QVERIFY(c2);

    QVERIFY(!c1->isActive());
    QVERIFY(c2->isActive());

    // also create a sequence started spy as the touch event should be passed through
    QSignalSpy sequenceStartedSpy(m_touch, &Touch::sequenceStarted);
    QVERIFY(sequenceStartedSpy.isValid());

    quint32 timestamp = 1;
    waylandServer()->backend()->touchDown(1, c1->geometry().center(), timestamp++);
    QVERIFY(c1->isActive());

    QVERIFY(sequenceStartedSpy.wait());
    QCOMPARE(sequenceStartedSpy.count(), 1);

    // cleanup
    waylandServer()->backend()->touchCancel();
}

}

WAYLANDTEST_MAIN(KWin::TouchInputTest)
#include "touch_input_test.moc"
