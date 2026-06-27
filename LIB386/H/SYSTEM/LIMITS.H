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
/// switch (see RES_MAX_H in RES_CATALOG.H, kept in sync). 1080 enables 1080p.
#define ADELINE_MAX_Y_RES 1080
