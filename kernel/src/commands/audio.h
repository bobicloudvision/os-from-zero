#ifndef AUDIO_COMMANDS_H
#define AUDIO_COMMANDS_H

// Audio command functions
void cmd_beep(const char *args);
void cmd_tone(const char *args);
void cmd_stop(const char *args);
void cmd_play(const char *args);
void cmd_notes(const char *args);
void cmd_audio_test(const char *args);
void cmd_audio_debug(const char *args);

// Register all audio commands
void register_audio_commands(void);

#endif // AUDIO_COMMANDS_H 