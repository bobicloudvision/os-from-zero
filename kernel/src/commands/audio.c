#include "audio.h"
#include "../audio.h"
#include "../terminal.h"
#include "../string.h"
#include "../shell.h"
#include <stddef.h>

// Convert string to integer (simple implementation)
static int str_to_int(const char *str) {
    if (!str) return 0;
    
    int result = 0;
    int sign = 1;
    
    // Handle negative numbers
    if (*str == '-') {
        sign = -1;
        str++;
    }
    
    // Convert digits
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

// Extract two integers from arguments string
static void parse_two_ints(const char *args, int *first, int *second) {
    *first = 0;
    *second = 0;
    
    if (!args) return;
    
    // Skip leading whitespace
    while (*args == ' ' || *args == '\t') args++;
    
    // Parse first number
    *first = str_to_int(args);
    
    // Skip to next number
    while (*args && *args != ' ' && *args != '\t') args++;
    while (*args == ' ' || *args == '\t') args++;
    
    // Parse second number
    *second = str_to_int(args);
}

void cmd_beep(const char *args) {
    if (!args || *args == '\0') {
        // Default system beep
        audio_play_event(AUDIO_SYSTEM_BEEP);
        return;
    }
    
    int frequency, duration;
    parse_two_ints(args, &frequency, &duration);
    
    if (frequency < 20 || frequency > 20000) {
        terminal_print("Error: Frequency must be between 20 and 20000 Hz\n");
        return;
    }
    
    if (duration < 0) {
        terminal_print("Error: Duration cannot be negative\n");
        return;
    }
    
    if (duration == 0) {
        duration = 500; // Default duration
    }
    
    terminal_print("Playing beep at ");
    char freq_str[16];
    int_to_string(frequency, freq_str);
    terminal_print(freq_str);
    terminal_print(" Hz for ");
    char dur_str[16];
    int_to_string(duration, dur_str);
    terminal_print(dur_str);
    terminal_print(" ms\n");
    
    audio_beep(frequency, duration);
}

void cmd_tone(const char *args) {
    if (!args || *args == '\0') {
        terminal_print("Usage: tone <frequency>\n");
        terminal_print("Example: tone 440 (plays A4 note continuously)\n");
        terminal_print("Use 'stop' command to stop the tone\n");
        return;
    }
    
    int frequency = str_to_int(args);
    
    if (frequency < 20 || frequency > 20000) {
        terminal_print("Error: Frequency must be between 20 and 20000 Hz\n");
        return;
    }
    
    terminal_print("Playing continuous tone at ");
    char freq_str[16];
    int_to_string(frequency, freq_str);
    terminal_print(freq_str);
    terminal_print(" Hz\n");
    terminal_print("Use 'stop' command to stop the tone\n");
    
    audio_play_tone(frequency);
}

void cmd_stop(const char *args) {
    (void)args; // Unused parameter
    audio_stop();
    terminal_print("Audio stopped\n");
}

void cmd_play(const char *args) {
    if (!args || *args == '\0') {
        terminal_print("Available melodies:\n");
        terminal_print("  startup  - Boot melody\n");
        terminal_print("  shutdown - Shutdown melody\n");
        terminal_print("  scale    - C major scale\n");
        terminal_print("  twinkle  - Twinkle twinkle little star\n");
        terminal_print("Usage: play <melody>\n");
        return;
    }
    
    if (strcmp(args, "startup") == 0) {
        terminal_print("Playing startup melody...\n");
        audio_play_event(AUDIO_STARTUP_SOUND);
    } else if (strcmp(args, "shutdown") == 0) {
        terminal_print("Playing shutdown melody...\n");
        audio_play_event(AUDIO_SHUTDOWN_SOUND);
    } else if (strcmp(args, "scale") == 0) {
        terminal_print("Playing C major scale...\n");
        audio_note_t scale[] = {
            {NOTE_C4, 300},
            {NOTE_D4, 300},
            {NOTE_E4, 300},
            {NOTE_F4, 300},
            {NOTE_G4, 300},
            {NOTE_A4, 300},
            {NOTE_B4, 300},
            {NOTE_C5, 600}
        };
        audio_play_melody(scale, sizeof(scale) / sizeof(scale[0]));
    } else if (strcmp(args, "twinkle") == 0) {
        terminal_print("Playing Twinkle Twinkle Little Star...\n");
        audio_note_t twinkle[] = {
            {NOTE_C4, 400}, {NOTE_C4, 400}, {NOTE_G4, 400}, {NOTE_G4, 400},
            {NOTE_A4, 400}, {NOTE_A4, 400}, {NOTE_G4, 800},
            {NOTE_F4, 400}, {NOTE_F4, 400}, {NOTE_E4, 400}, {NOTE_E4, 400},
            {NOTE_D4, 400}, {NOTE_D4, 400}, {NOTE_C4, 800}
        };
        audio_play_melody(twinkle, sizeof(twinkle) / sizeof(twinkle[0]));
    } else {
        terminal_print("Unknown melody: ");
        terminal_print(args);
        terminal_print("\nUse 'play' without arguments to see available melodies\n");
    }
}

void cmd_notes(const char *args) {
    (void)args; // Unused parameter
    terminal_print("Musical Notes and Frequencies:\n\n");
    terminal_print("Note  Frequency (Hz)\n");
    terminal_print("C4    262\n");
    terminal_print("D4    294\n");
    terminal_print("E4    330\n");
    terminal_print("F4    349\n");
    terminal_print("G4    392\n");
    terminal_print("A4    440\n");
    terminal_print("B4    494\n");
    terminal_print("C5    523\n");
    terminal_print("D5    587\n");
    terminal_print("E5    659\n");
    terminal_print("F5    698\n");
    terminal_print("G5    784\n");
    terminal_print("A5    880\n");
    terminal_print("B5    988\n");
    terminal_print("C6    1047\n\n");
    terminal_print("Use 'beep <frequency> <duration>' to play custom tones\n");
}

void cmd_audio_test(const char *args) {
    (void)args; // Unused parameter
    terminal_print("Testing audio system...\n");
    
    terminal_print("1. System beep... ");
    audio_play_event(AUDIO_SYSTEM_BEEP);
    terminal_print("Done!\n");
    
    terminal_print("2. Error beep... ");
    audio_play_event(AUDIO_ERROR_BEEP);
    terminal_print("Done!\n");
    
    terminal_print("3. Frequency sweep (200Hz to 2000Hz)...\n");
    for (int freq = 200; freq <= 2000; freq += 200) {
        audio_beep(freq, 100);
    }
    terminal_print("Done!\n");
    
    terminal_print("Audio test complete!\n");
}

void cmd_audio_debug(const char *args) {
    (void)args; // Unused parameter
    terminal_print("Running low-level audio hardware test...\n");
    terminal_print("This will test direct PIT and PC Speaker access.\n");
    terminal_print("You should hear a 1000 Hz tone for about 1 second.\n");
    terminal_print("Starting test...\n");
    
    audio_debug_test();
    
    terminal_print("Hardware test complete!\n");
    terminal_print("If you didn't hear a tone, there may be a QEMU configuration issue.\n");
}

// Register all audio commands
void register_audio_commands(void) {
    register_command("beep", cmd_beep, "Play a beep tone", "beep [frequency] [duration]", "Audio");
    register_command("tone", cmd_tone, "Play continuous tone", "tone <frequency>", "Audio");
    register_command("stop", cmd_stop, "Stop audio playback", "stop", "Audio");
    register_command("play", cmd_play, "Play predefined melodies", "play <melody>", "Audio");
    register_command("notes", cmd_notes, "Show musical note frequencies", "notes", "Audio");
    register_command("audiotest", cmd_audio_test, "Test audio system", "audiotest", "Audio");
    register_command("audiodebug", cmd_audio_debug, "Low-level hardware test", "audiodebug", "Audio");
} 