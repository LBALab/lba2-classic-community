#ifndef VIDEO_FIT_H
#define VIDEO_FIT_H

/* Destination rectangle for the 320x200 Smacker cinematic upscale.

   Two policies, selected by fitToFrame (the VideoFullScreen cfg flag):

     0 - centred letterbox (legacy): fixed 2x upscale to 640x400, centred in
         the framebuffer with black bars. Byte-identical to the pre-fit player
         at every resolution (2.0x/2.0x, 40px top/bottom bands at 640x480).

     1 - fit to screen: the largest UNIFORM scale that fits the source inside
         modeX x modeY without distortion, then centred. The limiting axis is
         filled exactly and the other is pillar/letter-boxed with black. At
         640x480 min(640/320, 480/200) == 2.0, so this collapses to the exact
         legacy layout -- the golden FMV frame is unchanged whichever branch is
         active.

   Self-contained (plain int, no engine headers) so the host rect-math test can
   share this single source of truth with PLAYACF.CPP. */
static inline void Video_ComputeCineRect(int modeX, int modeY, int fitToFrame,
                                         int *dstW, int *dstH,
                                         int *dstX0, int *dstY0) {
    const double srcW = 320.0;
    const double srcH = 200.0;

    double scale;
    if (fitToFrame) {
        const double sx = (double)modeX / srcW;
        const double sy = (double)modeY / srcH;
        scale = (sx < sy) ? sx : sy;
    } else {
        scale = 2.0;
    }

    int w = (int)(srcW * scale + 0.5);
    int h = (int)(srcH * scale + 0.5);

    /* Guard the limiting axis against a float round-up spilling one row/column
       past the framebuffer (would corrupt memory in the direct-to-Log blit). */
    if (w > modeX)
        w = modeX;
    if (h > modeY)
        h = modeY;

    *dstW = w;
    *dstH = h;
    *dstX0 = (modeX - w) / 2;
    *dstY0 = (modeY - h) / 2;
}

#endif // VIDEO_FIT_H
