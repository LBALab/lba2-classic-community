add_library(video_audio_resample VIDEO_AUDIO_RESAMPLE.CPP)
target_include_directories(video_audio_resample PUBLIC
  ${CMAKE_SOURCE_DIR}/LIB386/H
)
