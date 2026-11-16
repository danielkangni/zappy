#include "app/Fullscreen.hpp"
#include <X11/Xlib.h>
#include <X11/Xatom.h>

void setWindowFullscreen(unsigned long x11Window, bool on)
{
    Display* d = XOpenDisplay(nullptr);
    if (!d) return;

    Atom wmState   = XInternAtom(d, "_NET_WM_STATE", False);
    Atom maxVert   = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    Atom maxHorz   = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False);

    XEvent e{};
    e.type                 = ClientMessage;
    e.xclient.window       = (Window)x11Window;
    e.xclient.message_type = wmState;
    e.xclient.format       = 32;
    e.xclient.data.l[0]    = on ? 1 : 0;
    e.xclient.data.l[1]    = (long)maxVert;
    e.xclient.data.l[2]    = (long)maxHorz;
    e.xclient.data.l[3]    = 1;

    XSendEvent(d, DefaultRootWindow(d), False,
               SubstructureNotifyMask | SubstructureRedirectMask, &e);
    XFlush(d);
    XCloseDisplay(d);
}
