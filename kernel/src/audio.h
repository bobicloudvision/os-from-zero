#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>

// Structure for musical notes
typedef struct {
    uint16_t frequency;     // Frequency in Hz (0 for rest/silence)
    uint32_t duration_ms;   // Duration in milliseconds
} audio_note_t;

// Common musical note frequencies (in Hz)
#define NOTE_C4     262
#define NOTE_D4     294
#define NOTE_E4     330
#define NOTE_F4     349
#define NOTE_G4     392
#define NOTE_A4     440
#define NOTE_B4     494
#define NOTE_C5     523
#define NOTE_D5     587
#define NOTE_E5     659
#define NOTE_F5     698
#define NOTE_G5     784
#define NOTE_A5     880
#define NOTE_B5     988
#define NOTE_C6     1047

// Rest note
#define NOTE_REST   0

// Initialize audio system
void audio_init(void);

// Play a beep at specified frequency for specified duration
// frequency: Hz (20-20000), duration_ms: milliseconds (0 for continuous)
void audio_beep(uint16_t frequency, uint32_t duration_ms);

// Stop any currently playing audio
void audio_stop(void);

// Play a continuous tone until stopped
void audio_play_tone(uint16_t frequency);

// Play a melody from an array of notes
void audio_play_melody(const audio_note_t *notes, size_t count);

// Audio event types
typedef enum {
    AUDIO_SYSTEM_BEEP,
    AUDIO_ERROR_BEEP,
    AUDIO_STARTUP_SOUND,
    AUDIO_SHUTDOWN_SOUND
} audio_event_type_t;

// Play predefined audio events
void audio_play_event(audio_event_type_t event_type);

// Debug function
void audio_debug_test(void);     // Direct hardware test

#endif // AUDIO_H 