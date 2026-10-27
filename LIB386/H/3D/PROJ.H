#pragma once

#include <SYSTEM/ADELINE_TYPES.H>

// -----------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
enum ProjectionType { TYPE_3D = 0,
                      TYPE_ISO = 1 };

// -----------------------------------------------------------------------------
extern void SetProjection(S32 xcentre, S32 ycentre, S32 clip, S32 factorx,
                          S32 factory);
//! Isometric view depth mapped to aZ, the depth the GL scene stores and the
//! stereo vertex stages turn into an eye offset.
//!
//! An interior has no perspective camera: its "depth" is the isometric
//! painter key (x + z) / 2, and that key's natural scale lands every plane of
//! a room within a couple of pixels of eye separation, so the headset sees the
//! room as a flat backdrop behind the models.  Dividing here multiplies the eye
//! offset -- which goes as 1/aZ -- for the model polygons and for the
//! background depth bands alike, so both keep one depth scale.  Depth-buffer
//! ordering is unchanged: the map stays monotonic.
#define ISO_DEPTH_AZ_DENOM 16

//! Apply the same interior stereo gain to a value already on the aZ scale
//! (the shadow polygons carry GET_ZO depths, not painter keys).
static inline U16 IsoAzGain(S32 az) {
    if (az > 0) {
        az = (az + ISO_DEPTH_AZ_DENOM / 2) / ISO_DEPTH_AZ_DENOM;
    } else {
        az = az / ISO_DEPTH_AZ_DENOM;
    }
    if (az < 0) {
        az = 0;
    } else if (az > 65535) {
        az = 65535;
    }
    return (U16)az;
}

static inline U16 IsoDepthToAz(S32 depth) { return IsoAzGain(32768 - depth); }

extern void SetIsoProjection(S32 xcentre, S32 ycentre);

// =============================================================================
#ifdef __cplusplus
}
#endif
