# CPP-only tests for video audio resampling. No ASM, no game assets.
add_asm_cpp_test(
  NAME test_video_audio_resample
  TEST_SOURCE test_video_audio_resample.cpp
  LIBS video_audio_resample
)
