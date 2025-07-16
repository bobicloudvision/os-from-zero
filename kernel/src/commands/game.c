#include "game.h"
#include "system.h"
#include "../terminal.h"
#include "../keyboard.h"
#include "../audio.h"
#include "../string.h"
#include "../mouse.h"
#include <stddef.h>
#include <stdbool.h>

// Simple pseudo-random number generator
static uint32_t game_seed = 1;

static uint32_t simple_random(void) {
    game_seed = game_seed * 1103515245 + 12345;
    return (game_seed >> 16) & 0x7fff;
}

static void seed_random(uint32_t seed) {
    game_seed = seed;
}

// Convert string to integer
static int string_to_int(const char *str) {
    int result = 0;
    int sign = 1;
    
    if (*str == '-') {
        sign = -1;
        str++;
    }
    
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return result * sign;
}

// int_to_string is already defined in string.h

// Guess the Number game
static void cmd_guess_number(const char *args) {
    (void)args; // Mark unused parameter
    terminal_print("=== GUESS THE NUMBER GAME ===\n");
    terminal_print("I'm thinking of a number between 1 and 100!\n");
    terminal_print("You have 7 attempts to guess it.\n");
    terminal_print("Type your guess and press Enter.\n");
    terminal_print("Type 'quit' to exit the game.\n\n");
    
    // Seed random number generator with mouse position for better randomness
    mouse_state_t *mouse = mouse_get_state();
    seed_random(mouse->x + mouse->y * 13 + 42);
    
    int target = (simple_random() % 100) + 1;
    int attempts = 0;
    const int max_attempts = 7;
    bool won = false;
    
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    while (attempts < max_attempts && !won) {
        terminal_print("Attempt ");
        char attempt_str[4];
        int_to_string(attempts + 1, attempt_str);
        terminal_print(attempt_str);
        terminal_print("/");
        int_to_string(max_attempts, attempt_str);
        terminal_print(attempt_str);
        terminal_print(" - Enter your guess: ");
        
        // Read user input
        char input[32];
        size_t input_pos = 0;
        
        while (1) {
            char c = read_key();
            
            if (c == '\n') {
                input[input_pos] = '\0';
                terminal_putchar('\n');
                break;
            } else if (c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                    terminal_putchar('\b');
                }
            } else if (c >= '0' && c <= '9' && input_pos < sizeof(input) - 1) {
                input[input_pos++] = c;
                terminal_putchar(c);
            } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                // Allow letters for 'quit' command
                if (input_pos < sizeof(input) - 1) {
                    input[input_pos++] = c;
                    terminal_putchar(c);
                }
            }
        }
        
        // Check for quit command
        if (strcmp(input, "quit") == 0) {
            terminal_print("Thanks for playing! The number was ");
            char target_str[4];
            int_to_string(target, target_str);
            terminal_print(target_str);
            terminal_print(".\n");
            audio_play_event(AUDIO_ERROR_BEEP);
            return;
        }
        
        int guess = string_to_int(input);
        attempts++;
        
        if (guess == target) {
            terminal_print("🎉 CONGRATULATIONS! You guessed it!\n");
            terminal_print("The number was ");
            char target_str[4];
            int_to_string(target, target_str);
            terminal_print(target_str);
            terminal_print(" and you got it in ");
            int_to_string(attempts, attempt_str);
            terminal_print(attempt_str);
            terminal_print(" attempts!\n");
            
            // Play victory sound
            audio_beep(523, 200);  // C5
            audio_beep(659, 200);  // E5
            audio_beep(784, 200);  // G5
            audio_beep(1047, 400); // C6
            
            won = true;
        } else if (guess < target) {
            terminal_print("Too low! Try a higher number.\n");
            audio_beep(300, 150);  // Low tone
        } else {
            terminal_print("Too high! Try a lower number.\n");
            audio_beep(600, 150);  // High tone
        }
        
        // Give hints for remaining attempts
        if (!won && attempts < max_attempts) {
            int remaining = max_attempts - attempts;
            terminal_print("You have ");
            char remaining_str[4];
            int_to_string(remaining, remaining_str);
            terminal_print(remaining_str);
            terminal_print(" attempts left.\n");
            
            // Give range hints
            if (attempts >= 3) {
                terminal_print("Hint: The number is ");
                if (target <= 25) {
                    terminal_print("between 1 and 25");
                } else if (target <= 50) {
                    terminal_print("between 26 and 50");
                } else if (target <= 75) {
                    terminal_print("between 51 and 75");
                } else {
                    terminal_print("between 76 and 100");
                }
                terminal_print(".\n");
            }
        }
        
        terminal_print("\n");
    }
    
    if (!won) {
        terminal_print("😞 Game Over! You ran out of attempts.\n");
        terminal_print("The number was ");
        char target_str[4];
        int_to_string(target, target_str);
        terminal_print(target_str);
        terminal_print(". Better luck next time!\n");
        
        // Play game over sound
        audio_beep(200, 300);
        audio_beep(180, 300);
        audio_beep(160, 500);
    }
    
    terminal_print("\nThanks for playing! Type 'guess' to play again.\n");
}

// Tic-Tac-Toe game
static char ttt_board[9] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
static char current_player = 'X';

static void draw_ttt_board(void) {
    terminal_print("\n   |   |   \n");
    terminal_print(" ");
    terminal_putchar(ttt_board[0]);
    terminal_print(" | ");
    terminal_putchar(ttt_board[1]);
    terminal_print(" | ");
    terminal_putchar(ttt_board[2]);
    terminal_print(" \n");
    terminal_print("___|___|___\n");
    terminal_print("   |   |   \n");
    terminal_print(" ");
    terminal_putchar(ttt_board[3]);
    terminal_print(" | ");
    terminal_putchar(ttt_board[4]);
    terminal_print(" | ");
    terminal_putchar(ttt_board[5]);
    terminal_print(" \n");
    terminal_print("___|___|___\n");
    terminal_print("   |   |   \n");
    terminal_print(" ");
    terminal_putchar(ttt_board[6]);
    terminal_print(" | ");
    terminal_putchar(ttt_board[7]);
    terminal_print(" | ");
    terminal_putchar(ttt_board[8]);
    terminal_print(" \n");
    terminal_print("   |   |   \n\n");
}

static bool check_ttt_win(char player) {
    // Check rows
    for (int i = 0; i < 3; i++) {
        if (ttt_board[i*3] == player && ttt_board[i*3+1] == player && ttt_board[i*3+2] == player) {
            return true;
        }
    }
    
    // Check columns
    for (int i = 0; i < 3; i++) {
        if (ttt_board[i] == player && ttt_board[i+3] == player && ttt_board[i+6] == player) {
            return true;
        }
    }
    
    // Check diagonals
    if (ttt_board[0] == player && ttt_board[4] == player && ttt_board[8] == player) {
        return true;
    }
    if (ttt_board[2] == player && ttt_board[4] == player && ttt_board[6] == player) {
        return true;
    }
    
    return false;
}

static bool is_ttt_board_full(void) {
    for (int i = 0; i < 9; i++) {
        if (ttt_board[i] == ' ') {
            return false;
        }
    }
    return true;
}

static void cmd_tictactoe(const char *args) {
    (void)args; // Mark unused parameter
    terminal_print("=== TIC-TAC-TOE GAME ===\n");
    terminal_print("Player X starts first.\n");
    terminal_print("Enter position (1-9) to place your mark:\n");
    terminal_print("Positions:\n");
    terminal_print(" 1 | 2 | 3 \n");
    terminal_print("-----------\n");
    terminal_print(" 4 | 5 | 6 \n");
    terminal_print("-----------\n");
    terminal_print(" 7 | 8 | 9 \n");
    terminal_print("Type 'quit' to exit the game.\n");
    
    // Initialize board
    for (int i = 0; i < 9; i++) {
        ttt_board[i] = ' ';
    }
    current_player = 'X';
    
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    while (1) {
        draw_ttt_board();
        
        terminal_print("Player ");
        terminal_putchar(current_player);
        terminal_print("'s turn - Enter position (1-9): ");
        
        char input[32];
        size_t input_pos = 0;
        
        while (1) {
            char c = read_key();
            
            if (c == '\n') {
                input[input_pos] = '\0';
                terminal_putchar('\n');
                break;
            } else if (c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                    terminal_putchar('\b');
                }
            } else if (c >= '1' && c <= '9' && input_pos < sizeof(input) - 1) {
                input[input_pos++] = c;
                terminal_putchar(c);
            } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                if (input_pos < sizeof(input) - 1) {
                    input[input_pos++] = c;
                    terminal_putchar(c);
                }
            }
        }
        
        if (strcmp(input, "quit") == 0) {
            terminal_print("Thanks for playing!\n");
            audio_play_event(AUDIO_ERROR_BEEP);
            return;
        }
        
        int position = string_to_int(input);
        
        if (position < 1 || position > 9) {
            terminal_print("Invalid position! Use 1-9.\n");
            audio_beep(200, 100);
            continue;
        }
        
        int board_pos = position - 1;
        
        if (ttt_board[board_pos] != ' ') {
            terminal_print("Position already taken! Choose another.\n");
            audio_beep(200, 100);
            continue;
        }
        
        ttt_board[board_pos] = current_player;
        audio_beep(440, 100);
        
        if (check_ttt_win(current_player)) {
            draw_ttt_board();
            terminal_print("🎉 Player ");
            terminal_putchar(current_player);
            terminal_print(" wins!\n");
            
            // Play victory sound
            audio_beep(523, 150);  // C5
            audio_beep(659, 150);  // E5
            audio_beep(784, 150);  // G5
            audio_beep(1047, 300); // C6
            
            terminal_print("Type 'tictactoe' to play again!\n");
            return;
        }
        
        if (is_ttt_board_full()) {
            draw_ttt_board();
            terminal_print("It's a tie! Good game!\n");
            audio_beep(400, 200);
            audio_beep(400, 200);
            terminal_print("Type 'tictactoe' to play again!\n");
            return;
        }
        
        current_player = (current_player == 'X') ? 'O' : 'X';
    }
}

// Simple Rock Paper Scissors game
static void cmd_rockpaperscissors(const char *args) {
    (void)args; // Mark unused parameter
    terminal_print("=== ROCK PAPER SCISSORS ===\n");
    terminal_print("Choose your weapon:\n");
    terminal_print("1. Rock\n");
    terminal_print("2. Paper\n");
    terminal_print("3. Scissors\n");
    terminal_print("Type 'quit' to exit.\n\n");
    
    // Seed random with mouse position
    mouse_state_t *mouse = mouse_get_state();
    seed_random(mouse->x * mouse->y + 17);
    
    int player_wins = 0;
    int computer_wins = 0;
    
    audio_play_event(AUDIO_STARTUP_SOUND);
    
    while (1) {
        terminal_print("Score - You: ");
        char score_str[4];
        int_to_string(player_wins, score_str);
        terminal_print(score_str);
        terminal_print(" | Computer: ");
        int_to_string(computer_wins, score_str);
        terminal_print(score_str);
        terminal_print("\n");
        
        terminal_print("Enter your choice (1-3): ");
        
        char input[32];
        size_t input_pos = 0;
        
        while (1) {
            char c = read_key();
            
            if (c == '\n') {
                input[input_pos] = '\0';
                terminal_putchar('\n');
                break;
            } else if (c == '\b') {
                if (input_pos > 0) {
                    input_pos--;
                    terminal_putchar('\b');
                }
            } else if (c >= '1' && c <= '3' && input_pos < sizeof(input) - 1) {
                input[input_pos++] = c;
                terminal_putchar(c);
            } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                if (input_pos < sizeof(input) - 1) {
                    input[input_pos++] = c;
                    terminal_putchar(c);
                }
            }
        }
        
        if (strcmp(input, "quit") == 0) {
            terminal_print("Final Score - You: ");
            int_to_string(player_wins, score_str);
            terminal_print(score_str);
            terminal_print(" | Computer: ");
            int_to_string(computer_wins, score_str);
            terminal_print(score_str);
            terminal_print("\n");
            
            if (player_wins > computer_wins) {
                terminal_print("🎉 You won overall! Great job!\n");
                audio_beep(523, 200);
                audio_beep(659, 200);
                audio_beep(784, 300);
            } else if (computer_wins > player_wins) {
                terminal_print("Computer won overall. Better luck next time!\n");
                audio_beep(200, 500);
            } else {
                terminal_print("It's a tie overall! Well played!\n");
                audio_beep(400, 300);
            }
            
            terminal_print("Thanks for playing!\n");
            return;
        }
        
        int player_choice = string_to_int(input);
        
        if (player_choice < 1 || player_choice > 3) {
            terminal_print("Invalid choice! Use 1-3.\n");
            audio_beep(200, 100);
            continue;
        }
        
        int computer_choice = (simple_random() % 3) + 1;
        
        const char *choices[] = {"Rock", "Paper", "Scissors"};
        
        terminal_print("You chose: ");
        terminal_print(choices[player_choice - 1]);
        terminal_print("\n");
        
        terminal_print("Computer chose: ");
        terminal_print(choices[computer_choice - 1]);
        terminal_print("\n");
        
        if (player_choice == computer_choice) {
            terminal_print("It's a tie!\n");
            audio_beep(400, 200);
        } else if ((player_choice == 1 && computer_choice == 3) ||
                   (player_choice == 2 && computer_choice == 1) ||
                   (player_choice == 3 && computer_choice == 2)) {
            terminal_print("You win this round!\n");
            player_wins++;
            audio_beep(523, 200);
        } else {
            terminal_print("Computer wins this round!\n");
            computer_wins++;
            audio_beep(300, 200);
        }
        
        terminal_print("\n");
    }
}

// Initialize game commands
void init_game_commands(void) {
    register_command("guess", cmd_guess_number, 
                    "Play a number guessing game", 
                    "guess", 
                    "Games");
    
    register_command("tictactoe", cmd_tictactoe, 
                    "Play Tic-Tac-Toe", 
                    "tictactoe", 
                    "Games");
    
    register_command("rps", cmd_rockpaperscissors, 
                    "Play Rock Paper Scissors", 
                    "rps", 
                    "Games");
} 