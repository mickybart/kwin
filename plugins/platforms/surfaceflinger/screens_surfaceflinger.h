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
#ifndef KWIN_SCREENS_FRAMEBUFFER_H
#define KWIN_SCREENS_FRAMEBUFFER_H
#include "screens.h"

namespace KWin
{
class SurfaceFlingerBackend;

class SurfaceFlingerScreens : public BasicScreens
{
    Q_OBJECT
public:
    SurfaceFlingerScreens(SurfaceFlingerBackend *backend, QObject *parent = nullptr);
    virtual ~SurfaceFlingerScreens();
    float refreshRate(int screen) const override;

private:
    SurfaceFlingerBackend *m_backend;
};

}

#endif
