#include "DummyApp.h"

#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global"); // NOLINT

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

#endif

#if !defined(__ANDROID__)
#include <GL/glx.h>
#endif

#include <Eng/GameBase.h>
#include <Eng/Input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/ThreadWorker.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

namespace Ren {
extern Display  *g_dpy;
extern Window    g_win;
}

namespace {
DummyApp *g_app = nullptr;

const int KeycodeOffset = 8;

const unsigned char ScancodeToHID_table[256] = {
    0,   41, 30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  45,  46,  42,  43, 20, 26,
    8,   21, 23,  28,  24,  12,  18,  19,  47,  48,  158, 224, 4,   22,  7,   9,  10, 11,
    13,  14, 15,  51,  52,  53,  225, 49,  29,  27,  6,   25,  5,   17,  16,  54, 55, 56,
    229, 85, 226, 44,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  83, 71, 95,
    96,  97, 86,  92,  93,  94,  87,  89,  90,  91,  98,  99,  0,   0,   100, 68, 69, 0,
    0,   0,  0,   0,   0,   0,   88,  228, 84,  154, 230, 0,   74,  82,  75,  80, 79, 77,
    81,  78, 73,  76,  0,   0,   0,   0,   0,   103, 0,   72,  0,   0,   0,   0,  0,  227,
    231, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   118, 0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0};

uint32_t ScancodeToHID(uint32_t scancode) {
    if (scancode >= 256) {
        return 0;
    }
    return ScancodeToHID_table[scancode];
}
} // namespace

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

DummyApp::DummyApp() { g_app = this; }

DummyApp::~DummyApp() = default;

int DummyApp::Init(int w, int h, const char *) {
#if !defined(__ANDROID__)
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        fprintf(stderr, "dpy is null\n");
        return -1;
    }

    const int screen = XDefaultScreen(dpy_);
    Visual *visual = XDefaultVisual(dpy_, screen);
    const int depth  = DefaultDepth(dpy_, screen);

    XSetWindowAttributes swa;
    swa.colormap =
        XCreateColormap(dpy_, RootWindow(dpy_, screen), visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    win_ = XCreateWindow(dpy_, RootWindow(dpy_, screen), 0, 0, w, h, 0, depth,
                         InputOutput, visual,
                         CWBorderPixel | CWColormap | CWEventMask, &swa);

    Atom wm_delete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy_, win_, &wm_delete, 1);

    XMapWindow(dpy_, win_);
    XStoreName(dpy_, win_, "View");

    Ren::g_dpy = dpy_;
    Ren::g_win = win_;
#endif

    try {
        Viewer::PrepareAssets("pc");

        viewer_.reset(new Viewer(w, h, nullptr, nullptr, nullptr));

        auto input_manager = viewer_->GetComponent<InputManager>(INPUT_MANAGER_KEY);
        input_manager_ = input_manager;
    } catch (std::exception &e) {
        fprintf(stderr, "%s", e.what());
        return -1;
    }

    return 0;
}

void DummyApp::Destroy() {
    viewer_.reset();
#if !defined(__ANDROID__)
    XDestroyWindow(dpy_, win_);
    //XCloseDisplay(dpy_); // this is done in ContextVK.cpp (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
#endif
}

void DummyApp::Frame() { viewer_->Frame(); }

void DummyApp::Resize(int w, int h) { viewer_->Resize(w, h); }

void DummyApp::AddEvent(RawInputEv type, const uint32_t key_code, const float x,
                        const float y, const float dx, const float dy) {
    auto input_manager = viewer_->GetComponent<InputManager>(INPUT_MANAGER_KEY);
    if (!input_manager)
        return;

    InputManager::Event evt;
    evt.type = type;
    evt.key_code = key_code;
    evt.point.x = x;
    evt.point.y = y;
    evt.move.dx = dx;
    evt.move.dy = dy;
    evt.time_stamp = Sys::GetTimeUs();

    input_manager->AddRawInputEvent(evt);
}

#if !defined(__ANDROID__)
int DummyApp::Run(int argc, char *argv[]) {
    int w = 1024, h = 576;
    fullscreen_ = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--prepare_assets") == 0) {
            Viewer::PrepareAssets(argv[i + 1]);
            i++;
        } else if (strcmp(arg, "--norun") == 0) {
            return 0;
        } else if ((strcmp(arg, "--width") == 0 || strcmp(arg, "-w") == 0) &&
                   (i + 1 < argc)) {
            w = std::atoi(argv[++i]);
        } else if ((strcmp(arg, "--height") == 0 || strcmp(arg, "-h") == 0) &&
                   (i + 1 < argc)) {
            h = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--fullscreen") == 0 || strcmp(arg, "-fs") == 0) {
            fullscreen_ = true;
        }
    }

    if (Init(w, h, nullptr) < 0) {
        return -1;
    }

    __itt_thread_set_name("Main Thread");

    while (!terminated()) {
        __itt_frame_begin_v3(__g_itt_domain, nullptr);

        this->PollEvents();

        this->Frame();

#if defined(USE_GL_RENDER)
        uint64_t swap_start = Sys::GetTimeUs();
        glXSwapBuffers(dpy_, win_);
        uint64_t swap_end = Sys::GetTimeUs();

        auto swap_interval = viewer_->GetComponent<TimeInterval>(SWAP_TIMER_KEY);
        if (swap_interval) {
            swap_interval->start_timepoint_us = swap_start;
            swap_interval->end_timepoint_us = swap_end;
        }
#elif defined(USE_SW_RENDER)
        // TODO
#endif
        __itt_frame_end_v3(__g_itt_domain, nullptr);
    }

    this->Destroy();

    return 0;
}

#undef None

void DummyApp::PollEvents() {
    std::shared_ptr<InputManager> input_manager = input_manager_.lock();
    if (!input_manager)
        return;

    static float last_p1_pos[2] = {0.0f, 0.0f};
    static int last_window_size[2] = {0, 0};

    XEvent xev;
    while (XCheckWindowEvent(dpy_, win_,
                             (ExposureMask | StructureNotifyMask | KeyPressMask |
                              KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask),
                             &xev)) {
        InputManager::Event evt;

        if (xev.type == KeyPress) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            if (key_code == KeyEscape) {
                quit_ = true;
            } else {
                evt.type = RawInputEv::KeyDown;
                evt.key_code = key_code;
            }
        } else if (xev.type == KeyRelease) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            evt.type = RawInputEv::KeyUp;
            evt.key_code = key_code;
        } else if (xev.type == ButtonPress &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = RawInputEv::P1Down;
            } else if (xev.xbutton.button == Button3) {
                evt.type = RawInputEv::P2Down;
            } else if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
                evt.type = RawInputEv::MouseWheel;
                evt.move.dx = (xev.xbutton.button == Button4) ? 1.0f : -1.0f;
            }
            evt.point.x = float(xev.xbutton.x);
            evt.point.y = float(xev.xbutton.y);
        } else if (xev.type == ButtonRelease &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = RawInputEv::P1Up;
            } else if (xev.xbutton.button == Button3) {
                evt.type = RawInputEv::P2Up;
            }
            evt.point.x = float(xev.xbutton.x);
            evt.point.y = float(xev.xbutton.y);
        } else if (xev.type == MotionNotify) {
            evt.type = RawInputEv::P1Move;
            evt.point.x = float(xev.xmotion.x);
            evt.point.y = float(xev.xmotion.y);
            evt.move.dx = evt.point.x - last_p1_pos[0];
            evt.move.dy = evt.point.y - last_p1_pos[1];

            last_p1_pos[0] = evt.point.x;
            last_p1_pos[1] = evt.point.y;
        } else if (xev.type == ConfigureNotify) {
            if (xev.xconfigure.width != last_window_size[0] ||
                xev.xconfigure.height != last_window_size[1]) {

                Resize(xev.xconfigure.width, xev.xconfigure.height);

                evt.type = RawInputEv::Resize;
                evt.point.x = (float)xev.xconfigure.width;
                evt.point.y = (float)xev.xconfigure.height;

                last_window_size[0] = xev.xconfigure.width;
                last_window_size[1] = xev.xconfigure.height;
            }
        }

        if (evt.type != RawInputEv::None) {
            evt.time_stamp = Sys::GetTimeUs();
            input_manager->AddRawInputEvent(evt);
        }
    }

    if (XCheckTypedWindowEvent(dpy_, win_, ClientMessage, &xev)) {
        if (strcmp(XGetAtomName(dpy_, xev.xclient.message_type), "WM_PROTOCOLS") == 0) {
            quit_ = true;
        }
    }
}

#endif
