#pragma once
#include "flight.h"

// Built-in sample flights (demo mode, and the serial `n` command), in the
// same JSON shape as the endpoint.

// The next sample, in turn.
Flight demoNext();
// Nudge the numbers a little, as if time passed (exercises partial refresh).
void demoNudge(Flight &f);
