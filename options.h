/*****************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 1999, 2000 Matthias Ettrich <ettrich@kde.org>
Copyright (C) 2003 Lubos Lunak <l.lunak@kde.org>

You can Freely distribute this program under the GNU General Public
License. See the file "COPYING" for the exact licensing terms.
******************************************************************/

#ifndef KWIN_OPTIONS_H
#define KWIN_OPTIONS_H

#include <qobject.h>
#include <qfont.h>
#include <qpalette.h>
#include <qstringlist.h>
#include <kdecoration_p.h>

namespace KWinInternal
{

class Options : public KDecorationOptions 
    {
    public:

        Options();
        ~Options();

        virtual unsigned long updateSettings();

        /*!
          Different focus policies:
          <ul>

          <li>ClickToFocus - Clicking into a window activates it. This is
          also the default.

          <li>FocusFollowsMouse - Moving the mouse pointer actively onto a
          normal window activates it. For convenience, the desktop and
          windows on the dock are excluded. They require clicking.

          <li>FocusUnderMouse - The window that happens to be under the
          mouse pointer becomes active. The invariant is: no window can
          have focus that is not under the mouse. This also means that
          Alt-Tab won't work properly and popup dialogs are usually
          unsable with the keyboard. Note that the desktop and windows on
          the dock are excluded for convenience. They get focus only when
          clicking on it.

          <li>FocusStrictlyUnderMouse - this is even worse than
          FocusUnderMouse. Only the window under the mouse pointer is
          active. If the mouse points nowhere, nothing has the focus. If
          the mouse points onto the desktop, the desktop has focus. The
          same holds for windows on the dock.

          Note that FocusUnderMouse and FocusStrictlyUnderMouse are not
          particulary useful. They are only provided for old-fashined
          die-hard UNIX people ;-)

          </ul>
         */
        enum FocusPolicy { ClickToFocus, FocusFollowsMouse, FocusUnderMouse, FocusStrictlyUnderMouse };
        FocusPolicy focusPolicy;


        /**
           Whether clicking on a window raises it in FocusFollowsMouse
           mode or not.
         */
        bool clickRaise;

        /**
           whether autoraise is enabled FocusFollowsMouse mode or not.
         */
        bool autoRaise;

        /**
           autoraise interval
         */
        int autoRaiseInterval;

        /**
           Whether shade hover is enabled or not
         */
        bool shadeHover;

        /**
           shade hover interval
         */
        int shadeHoverInterval;

        /**
           Different Alt-Tab-Styles:
           <ul>

           <li> KDE - the recommended KDE style. Alt-Tab opens a nice icon
           box that makes it easy to select the window you want to tab
           to. The order automatically adjusts to the most recently used
           windows. Note that KDE style does not work with the
           FocusUnderMouse and FocusStrictlyUnderMouse focus
           policies. Choose ClickToFocus or FocusFollowsMouse instead.

           <li> CDE - the old-fashion CDE style. Alt-Tab cycles between
           the windows in static order. The current window gets raised,
           the previous window gets lowered.

           </ul>
         */
        enum AltTabStyle { KDE, CDE };
        AltTabStyle altTabStyle;

        /**
         * Xinerama options
         */
        bool xineramaEnabled;
        bool xineramaPlacementEnabled;
        bool xineramaMovementEnabled;
        bool xineramaMaximizeEnabled;

        /**
           MoveResizeMode, either Tranparent or Opaque.
         */
        enum MoveResizeMode { Transparent, Opaque };

        MoveResizeMode resizeMode;
        MoveResizeMode moveMode;

        /**
         * Placement policies. How workspace decides the way windows get positioned
         * on the screen. The better the policy, the heavier the resource use.
         * Normally you don't have to worry. What the WM adds to the startup time
         * is nil compared to the creation of the window itself in the memory
         */
        enum PlacementPolicy { Random, Smart, Cascade, Centered, ZeroCornered };
        PlacementPolicy placement;

        enum DialogPlacementPolicy { ObeyApplication, DialogCentered, OnMainWindow, UnderMouse, DialogPlaced };
        DialogPlacementPolicy dialog_placement;

        bool focusPolicyIsReasonable() 
            {
            return focusPolicy == ClickToFocus || focusPolicy == FocusFollowsMouse;
            }

        /**
         * whether we animate the shading of windows to titlebar or not
         */
        bool animateShade;

        /**
         * the size of the zone that triggers snapping on desktop borders
         */
        int borderSnapZone;

        /**
         * the number of animation steps (would this be general?)
         */
        int windowSnapZone;


        /**
         * snap only when windows will overlap
         */
        bool snapOnlyWhenOverlapping;

        /**
         * whether we animate the minimization of windows or not
         */
        bool  animateMinimize;

        /**
         * Animation speed (0 .. 10 )
        */
        int animateMinimizeSpeed;

        /**
         * whether or not we roll over to the other edge when switching desktops past the edge
         */
        bool rollOverDesktops;

        // 0 - 4 , see Workspace::allowClientActivation()
        int focusStealingPreventionLevel;

        /**
         * List of window classes to ignore PPosition size hint
         */
        QStringList ignorePositionClasses;

        WindowOperation operationTitlebarDblClick() { return OpTitlebarDblClick; }

        enum MouseCommand 
            {
            MouseRaise, MouseLower, MouseOperationsMenu, MouseToggleRaiseAndLower,
            MouseActivateAndRaise, MouseActivateAndLower, MouseActivate,
            MouseActivateRaiseAndPassClick, MouseActivateAndPassClick,
            MouseMove, MouseUnrestrictedMove,
            MouseActivateRaiseAndMove, MouseActivateRaiseAndUnrestrictedMove,
            MouseResize, MouseUnrestrictedResize,
            MouseShade,
            MouseMinimize,
            MouseNothing
            };

        MouseCommand commandActiveTitlebar1() { return CmdActiveTitlebar1; }
        MouseCommand commandActiveTitlebar2() { return CmdActiveTitlebar2; }
        MouseCommand commandActiveTitlebar3() { return CmdActiveTitlebar3; }
        MouseCommand commandInactiveTitlebar1() { return CmdInactiveTitlebar1; }
        MouseCommand commandInactiveTitlebar2() { return CmdInactiveTitlebar2; }
        MouseCommand commandInactiveTitlebar3() { return CmdInactiveTitlebar3; }
        MouseCommand commandWindow1() { return CmdWindow1; }
        MouseCommand commandWindow2() { return CmdWindow2; }
        MouseCommand commandWindow3() { return CmdWindow3; }
        MouseCommand commandAll1() { return CmdAll1; }
        MouseCommand commandAll2() { return CmdAll2; }
        MouseCommand commandAll3() { return CmdAll3; }
        uint keyCmdAllModKey() { return CmdAllModKey; }


        static WindowOperation windowOperation(const QString &name, bool restricted );
        static MouseCommand mouseCommand(const QString &name, bool restricted );

        /**
        * @returns true if the Geometry Tip should be shown during a window move/resize.
        * @since 3.2
        */
        bool showGeometryTip();

        /**
        * @returns true if window button tooltips should use a fade-in effect
        * @see #animateTooltips
        * @see #showTooltips
        */
        bool fadeTooltips(); // FRAME tohle asi muze jit

        /**
        * @returns true if window button tooltips should use an animated effect.
        * When this is true, tooltips should use a animated scroll effect. If
        * fadeToolTips is also true, tooltips should be faded-in instead.
        * @see #fadeTooltips
        * @see #showTooltips
        */
        bool animateTooltips();

        enum { ElectricDisabled = 0, ElectricMoveOnly = 1, ElectricAlways = 2 };
        /**
        * @returns true if electric borders are enabled. With electric borders
        * you can change desktop by moving the mouse pointer towards the edge
        * of the screen
        */
        int electricBorders();

        /**
        * @returns the activation delay for electric borders in milliseconds.
        */
        int electricBorderDelay();

    private:
        WindowOperation OpTitlebarDblClick;

    // mouse bindings
        MouseCommand CmdActiveTitlebar1;
        MouseCommand CmdActiveTitlebar2;
        MouseCommand CmdActiveTitlebar3;
        MouseCommand CmdInactiveTitlebar1;
        MouseCommand CmdInactiveTitlebar2;
        MouseCommand CmdInactiveTitlebar3;
        MouseCommand CmdWindow1;
        MouseCommand CmdWindow2;
        MouseCommand CmdWindow3;
        MouseCommand CmdAll1;
        MouseCommand CmdAll2;
        MouseCommand CmdAll3;
        uint CmdAllModKey;

        bool fade_tooltips;
        bool animate_tooltips;
        int electric_borders;
        int electric_border_delay;
        bool show_geometry_tip;
    };

extern Options* options;

} // namespace

#endif
