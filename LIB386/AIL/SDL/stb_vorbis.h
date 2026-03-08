#ifndef STB_VORBIS_INCLUDE_STB_VORBIS_H
#define STB_VORBIS_INCLUDE_STB_VORBIS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Decode entire file to 16-bit interleaved PCM. Returns samples per channel, or -1 on error.
   *output is malloc'd; caller must free() it. */
extern int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output);

#ifdef __cplusplus
}
#endif

#endif
