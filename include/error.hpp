#ifndef SKUNKWORK_ERROR_HPP
#define SKUNKWORK_ERROR_HPP

#include <cstdlib>
#include <string>

#ifdef _WIN32

#include <windows.h>

void reportError(const std::string &err)
{
    // TODO:
    // Close/unfullscreen glfw to have popup on top?
    MessageBoxA(NULL, "Error", err.c_str(), MB_ICONERROR | MB_OK);
}

#else // !_WIN32

#include <cstdio>

void reportError(const std::string &err)
{
    fprintf(stderr, "%s\n", err.c_str());
}

#endif // _WIN32

#endif // SKUNKWORK_ERROR_HPP
