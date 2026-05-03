/**
 * @file setup_controller_globals.cpp
 * @brief Process-wide SetupController pointer for teardown and cross-module lookup.
 */

#include "setup_controller.h"

static SetupController *s_global_controller = nullptr;

SetupController *global_setup_controller() { return s_global_controller; }

void global_setup_controller_set(SetupController *c) {
  s_global_controller = c;
}
