#pragma once

/* First-person stereo body: a ghost copy of the real Twinsen body with the
 * head aligned to the tracked head and two independent arm IK solves.  The
 * gameplay body is never modified. */

void VrBody_Update(void);
void VrBody_DrawFirstPerson(void *ptrobj);
