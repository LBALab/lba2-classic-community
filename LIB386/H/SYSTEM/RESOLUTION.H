#pragma once

// Render-space framebuffer dimensions.
//
// These are the dimensions of the engine's internal framebuffer (the Log/Screen
// buffers allocated in MEM.CPP, the surface CreateScreenMemory configures, and
// the area the 3D pipeline and cinema bars cover). They are NOT the authored
// UI layout dimensions (which remain 640x480 by convention and are encoded as
// literals in UI code).
//
// Default 640x480 matches the original game. Overridable by the build system
// (e.g. -DRESOLUTION_X=...) to enable wider canvases. Anything other than the
// default is currently untested and requires additional work in the UI layer
// and pre-rendered art handling.

#ifndef RESOLUTION_X
#define RESOLUTION_X 640
#endif

#ifndef RESOLUTION_Y
#define RESOLUTION_Y 480
#endif

#if (RESOLUTION_X & 7)
#error Horizontal resolution must be a multiple of 8
#endif
