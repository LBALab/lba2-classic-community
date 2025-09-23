target_sources(ail PRIVATE CD.CPP COMMON.CPP MIDI.CPP SAMPLE.CPP STREAM.CPP)
target_compile_definitions(ail PUBLIC USE_SOUND_SDL)
