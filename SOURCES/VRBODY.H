#pragma once

/* First-person stereo body: a ghost copy of the real Twinsen body with the
 * head aligned to the tracked head and two independent arm IK solves.  The
 * gameplay body is never modified. */

void VrBody_Update(void);
void VrBody_DrawFirstPerson(void *ptrobj);

/* First-person physical throw.  The press arms the throw (BeginThrow)
 * instead of starting the hidden body's throw animation; the release turns
 * the aim and swing captured while held into a world-space launch
 * (EndThrow).  Both return 0 outside a first-person stereo session or when
 * no hand is tracked, so the caller falls back to the gameplay animation. */
S32 VrBody_BeginThrow(void);
S32 VrBody_EndThrow(S32 *x, S32 *y, S32 *z, S32 *alpha, S32 *beta);
