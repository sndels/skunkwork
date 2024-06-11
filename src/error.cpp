#include "error.hpp"

#include <cstdlib>

#if defined(_WIN32) && defined(DEMO_MODE)

namespace
{
std::string sErrors;
}

void reportError(const std::string &err)
{
    // Windows demo mode doesn't have a terminal so collect errors to be shown
    // on exit.
    sErrors += err;
    sErrors += '\n';
}

#include <windows.h>

// Should not be called while holding locks on files or other things that
// won't get automatically cleaned up on process exit.
void exitWithErrors(const Window *window)
{
    if (window != nullptr)
        // Unfullscreen so that our popup can be on top
        SDL_SetWindowFullscreen(window->ptr(), 0);
    // Windows demo mode doesn't have a terminal so we'll show the collected
    // errors in a popup
    MessageBoxA(
        NULL, sErrors.c_str(), "Error",
        MB_ICONERROR | MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
    // No cleanup, assume OS and drivers do their thing.
    exit(EXIT_FAILURE);
}

#else // !_WIN32

#include <cstdio>

void reportError(const std::string &err)
{
    fprintf(stderr, "%s\n", err.c_str());
}

void exitWithErrors(const Window * /*window*/) { exit(EXIT_FAILURE); }

#endif // _WIN32
