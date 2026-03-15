#ifndef STB_VORBIS_INCLUDE_STB_VORBIS_H
#define STB_VORBIS_INCLUDE_STB_VORBIS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stb_vorbis stb_vorbis;

typedef struct {
   char *alloc_buffer;
   int   alloc_buffer_length_in_bytes;
} stb_vorbis_alloc;

typedef struct {
   unsigned int sample_rate;
   int channels;
   unsigned int setup_memory_required;
   unsigned int setup_temp_memory_required;
   unsigned int temp_memory_required;
   int max_frame_size;
} stb_vorbis_info;

/* Decode entire file to 16-bit interleaved PCM. Returns samples per channel, or -1 on error.
   *output is malloc'd; caller must free() it. */
extern int stb_vorbis_decode_filename(const char *filename, int *channels, int *sample_rate, short **output);

/* Incremental decoding API */
extern stb_vorbis * stb_vorbis_open_filename(const char *filename,
                                  int *error, const stb_vorbis_alloc *alloc_buffer);
extern stb_vorbis_info stb_vorbis_get_info(stb_vorbis *f);
extern int stb_vorbis_get_samples_short_interleaved(stb_vorbis *f, int channels, short *buffer, int num_shorts);
extern void stb_vorbis_close(stb_vorbis *f);

#ifdef __cplusplus
}
#endif

#endif
