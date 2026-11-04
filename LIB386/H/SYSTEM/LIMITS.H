#pragma once

/// Max length of a path
#define ADELINE_MAX_PATH 256

/// Max char length for disk component of path (Eg: C:\, A:\, etc...)
#define ADELINE_MAX_DRIVE 3

/// Max char length for directory component of path
#define ADELINE_MAX_DIR 256

/// Max char length for filename component of path
#define ADELINE_MAX_FILENAME 256

/// Maximum screen resolution of Y axis. Sizes the static TabOffLine[] /
/// TabOffPhysLine[] line-offset tables and bounds the runtime resolution
/// switch (see RES_MAX_H in RES_CATALOG.H, kept in sync). 2160 covers 1080p
/// and the OpenXR swapchain-recommended size on Quest (1680x1760).
#define ADELINE_MAX_Y_RES 2160
