/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

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

#ifndef KWIN_UTILS_H
#define KWIN_UTILS_H

// cmake stuff
#include <config-kwin.h>
#include <kwinconfig.h>
// kwin
#include <kwinglobals.h>
// KDE
#include <netwm_def.h>
// Qt
#include <QLoggingCategory>
#include <QList>
#include <QPoint>
#include <QRect>
#include <QScopedPointer>
#include <QProcess>
// system
#include <limits.h>
Q_DECLARE_LOGGING_CATEGORY(KWIN_CORE)
namespace KWin
{

// window types that are supported as normal windows (i.e. KWin actually manages them)
const NET::WindowTypes SUPPORTED_MANAGED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
        | NET::UtilityMask | NET::SplashMask | NET::NotificationMask | NET::OnScreenDisplayMask;
// window types that are supported as unmanaged (mainly for compositing)
const NET::WindowTypes SUPPORTED_UNMANAGED_WINDOW_TYPES_MASK = NET::NormalMask | NET::DesktopMask | NET::DockMask
        | NET::ToolbarMask | NET::MenuMask | NET::DialogMask /*| NET::OverrideMask*/ | NET::TopMenuMask
        | NET::UtilityMask | NET::SplashMask | NET::DropdownMenuMask | NET::PopupMenuMask
        | NET::TooltipMask | NET::NotificationMask | NET::ComboBoxMask | NET::DNDIconMask | NET::OnScreenDisplayMask;

const QPoint invalidPoint(INT_MIN, INT_MIN);

class Toplevel;
class Client;
class Unmanaged;
class Deleted;
class Group;
class Options;

typedef QList< Toplevel* > ToplevelList;
typedef QList< Client* > ClientList;
typedef QList< const Client* > ConstClientList;
typedef QList< Unmanaged* > UnmanagedList;
typedef QList< Deleted* > DeletedList;

typedef QList< Group* > GroupList;

extern Options* options;

enum Layer {
    UnknownLayer = -1,
    FirstLayer = 0,
    DesktopLayer = FirstLayer,
    BelowLayer,
    NormalLayer,
    DockLayer,
    AboveLayer,
    NotificationLayer, // layer for windows of type notification
    ActiveLayer, // active fullscreen, or active dialog
    OnScreenDisplayLayer, // layer for On Screen Display windows such as volume feedback
    UnmanagedLayer, // layer for override redirect windows.
    NumLayers // number of layers, must be last
};

// yes, I know this is not 100% like standard operator++
inline void operator++(Layer& lay)
{
    lay = static_cast< Layer >(lay + 1);
}

enum StrutArea {
    StrutAreaInvalid = 0, // Null
    StrutAreaTop     = 1 << 0,
    StrutAreaRight   = 1 << 1,
    StrutAreaBottom  = 1 << 2,
    StrutAreaLeft    = 1 << 3,
    StrutAreaAll     = StrutAreaTop | StrutAreaRight | StrutAreaBottom | StrutAreaLeft
};
Q_DECLARE_FLAGS(StrutAreas, StrutArea)

class StrutRect : public QRect
{
public:
    explicit StrutRect(QRect rect = QRect(), StrutArea area = StrutAreaInvalid);
    StrutRect(const StrutRect& other);
    inline StrutArea area() const {
        return m_area;
    };
private:
    StrutArea m_area;
};
typedef QVector<StrutRect> StrutRects;


enum ShadeMode {
    ShadeNone, // not shaded
    ShadeNormal, // normally shaded - isShade() is true only here
    ShadeHover, // "shaded", but visible due to hover unshade
    ShadeActivated // "shaded", but visible due to alt+tab to the window
};

/**
 * Maximize mode. These values specify how a window is maximized.
 */
// these values are written to session files, don't change the order
enum MaximizeMode {
    MaximizeRestore    = 0, ///< The window is not maximized in any direction.
    MaximizeVertical   = 1, ///< The window is maximized vertically.
    MaximizeHorizontal = 2, ///< The window is maximized horizontally.
    /// Equal to @p MaximizeVertical | @p MaximizeHorizontal
    MaximizeFull = MaximizeVertical | MaximizeHorizontal
};

inline
MaximizeMode operator^(MaximizeMode m1, MaximizeMode m2)
{
    return MaximizeMode(int(m1) ^ int(m2));
}

template <typename T> using ScopedCPointer = QScopedPointer<T, QScopedPointerPodDeleter>;

void updateXTime();
void grabXServer();
void ungrabXServer();
bool grabbedXServer();
bool grabXKeyboard(xcb_window_t w = rootWindow());
void ungrabXKeyboard();

/**
 * Small helper class which performs @link grabXServer in the ctor and
 * @link ungrabXServer in the dtor. Use this class to ensure that grab and
 * ungrab are matched.
 *
 * To simplify usage consider using the macro GRAB_SERVER_DURING_CONTEXT
 **/
class XServerGrabber
{
public:
    XServerGrabber() {
        grabXServer();
    }
    ~XServerGrabber() {
        ungrabXServer();
    }
};

#define GRAB_SERVER_DURING_CONTEXT XServerGrabber xserverGrabber;

// the docs say it's UrgencyHint, but it's often #defined as XUrgencyHint
#ifndef UrgencyHint
#define UrgencyHint XUrgencyHint
#endif

// converting between X11 mouse/keyboard state mask and Qt button/keyboard states
int qtToX11Button(Qt::MouseButton button);
Qt::MouseButton x11ToQtMouseButton(int button);
int qtToX11State(Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
Qt::MouseButtons x11ToQtMouseButtons(int state);
Qt::KeyboardModifiers x11ToQtKeyboardModifiers(int state);

void checkNonExistentClients();

static inline int bitCount(uint32_t mask)
{
#if defined(__GNUC__)
    return __builtin_popcount(mask);
#else
    int count = 0;

    while (mask) {
        count += (mask & 1);
        mask >>= 1;
    }

    return count;
#endif
}


/**
 * Separate the concept of an unet QPoint and 0,0
 */
class ClearablePoint
{
public:
    inline bool isValid() const {
        return m_valid;
    }

    inline void clear(){
        m_valid = false;
    }

    inline void setPoint(const QPoint &point) {
        m_point = point; m_valid = true;
    }

    inline QPoint point() const {
        return m_point;
    }

private:
    QPoint m_point;
    bool m_valid = false;
};

/**
 * QProcess subclass which unblocks SIGUSR in the child process.
 **/
class KWIN_EXPORT Process : public QProcess
{
    Q_OBJECT
public:
    explicit Process(QObject *parent = nullptr);
    virtual ~Process();

protected:
    void setupChildProcess() override;
};

} // namespace

// Must be outside namespace
Q_DECLARE_OPERATORS_FOR_FLAGS(KWin::StrutAreas)

#endif
