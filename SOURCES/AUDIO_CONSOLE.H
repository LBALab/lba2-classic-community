#ifndef AUDIO_CONSOLE_H
#define AUDIO_CONSOLE_H

/* The console commands the audio module owns, registered from Console_RegisterAll
   beside the other modules' own registrations. Kept apart from AMBIANCE.H because
   the registration is the only thing outside this file that calls in. */
extern void Audio_RegisterCommands(void);

#endif /* AUDIO_CONSOLE_H */
