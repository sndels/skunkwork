#ifndef SKUNKWORK_ERROR_HPP
#define SKUNKWORK_ERROR_HPP

#include "window.hpp"
#include <string>

void reportError(const std::string &err);
void exitWithErrors(const Window *window);

#endif // SKUNKWORK_ERROR_HPP
