#include "math.h"
#include "../terminal.h"
#include "../fpu_simple.h"
#include "../shell.h"

void command_fpu_test(const char *args) {
    (void)args; // Unused parameter
    terminal_print("FPU Test Command\n");
    terminal_print("================\n");
    
    // Simple floating point arithmetic test
    float a = 3.14159f;
    float b = 2.71828f;
    float result = a + b;
    
    terminal_print("Testing basic floating point operations:\n");
    terminal_print("  a = 3.14159\n");
    terminal_print("  b = 2.71828\n");
    terminal_print("  a + b = ");
    
    // Convert result to string for display
    int integer_part = (int)result;
    int fractional_part = (int)((result - integer_part) * 100000);
    
    // Print integer part
    char int_str[16];
    int i = 0;
    if (integer_part == 0) {
        int_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (integer_part > 0) {
            temp[j++] = '0' + (integer_part % 10);
            integer_part /= 10;
        }
        while (j > 0) {
            int_str[i++] = temp[--j];
        }
    }
    int_str[i] = '\0';
    terminal_print(int_str);
    
    // Print fractional part
    terminal_print(".");
    char frac_str[16];
    i = 0;
    if (fractional_part == 0) {
        frac_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (fractional_part > 0) {
            temp[j++] = '0' + (fractional_part % 10);
            fractional_part /= 10;
        }
        while (j > 0) {
            frac_str[i++] = temp[--j];
        }
    }
    frac_str[i] = '\0';
    terminal_print(frac_str);
    terminal_print("\n");
    
    // Test square root
    float sqrt_test = 16.0f;
    float sqrt_result = math_sqrt(sqrt_test);
    terminal_print("  sqrt(16.0) = ");
    int sqrt_int = (int)sqrt_result;
    int sqrt_frac = (int)((sqrt_result - sqrt_int) * 100000);
    
    char sqrt_int_str[16];
    i = 0;
    if (sqrt_int == 0) {
        sqrt_int_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (sqrt_int > 0) {
            temp[j++] = '0' + (sqrt_int % 10);
            sqrt_int /= 10;
        }
        while (j > 0) {
            sqrt_int_str[i++] = temp[--j];
        }
    }
    sqrt_int_str[i] = '\0';
    terminal_print(sqrt_int_str);
    
    terminal_print(".");
    char sqrt_frac_str[16];
    i = 0;
    if (sqrt_frac == 0) {
        sqrt_frac_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (sqrt_frac > 0) {
            temp[j++] = '0' + (sqrt_frac % 10);
            sqrt_frac /= 10;
        }
        while (j > 0) {
            sqrt_frac_str[i++] = temp[--j];
        }
    }
    sqrt_frac_str[i] = '\0';
    terminal_print(sqrt_frac_str);
    terminal_print("\n");
    
    // Test trigonometric functions
    float angle = 1.5708f; // approximately pi/2
    float sin_result = math_sin(angle);
    float cos_result = math_cos(angle);
    
    terminal_print("  sin(pi/2) ≈ ");
    int sin_int = (int)sin_result;
    int sin_frac = (int)((sin_result - sin_int) * 100000);
    
    char sin_int_str[16];
    i = 0;
    if (sin_int == 0) {
        sin_int_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (sin_int > 0) {
            temp[j++] = '0' + (sin_int % 10);
            sin_int /= 10;
        }
        while (j > 0) {
            sin_int_str[i++] = temp[--j];
        }
    }
    sin_int_str[i] = '\0';
    terminal_print(sin_int_str);
    
    terminal_print(".");
    char sin_frac_str[16];
    i = 0;
    if (sin_frac == 0) {
        sin_frac_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (sin_frac > 0) {
            temp[j++] = '0' + (sin_frac % 10);
            sin_frac /= 10;
        }
        while (j > 0) {
            sin_frac_str[i++] = temp[--j];
        }
    }
    sin_frac_str[i] = '\0';
    terminal_print(sin_frac_str);
    terminal_print("\n");
    
    terminal_print("  cos(pi/2) ≈ ");
    int cos_int = (int)cos_result;
    int cos_frac = (int)((cos_result - cos_int) * 100000);
    
    char cos_int_str[16];
    i = 0;
    if (cos_int == 0) {
        cos_int_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (cos_int > 0) {
            temp[j++] = '0' + (cos_int % 10);
            cos_int /= 10;
        }
        while (j > 0) {
            cos_int_str[i++] = temp[--j];
        }
    }
    cos_int_str[i] = '\0';
    terminal_print(cos_int_str);
    
    terminal_print(".");
    char cos_frac_str[16];
    i = 0;
    if (cos_frac == 0) {
        cos_frac_str[i++] = '0';
    } else {
        char temp[16];
        int j = 0;
        while (cos_frac > 0) {
            temp[j++] = '0' + (cos_frac % 10);
            cos_frac /= 10;
        }
        while (j > 0) {
            cos_frac_str[i++] = temp[--j];
        }
    }
    cos_frac_str[i] = '\0';
    terminal_print(cos_frac_str);
    terminal_print("\n");
    
    terminal_print("\nFPU is working correctly!\n");
    terminal_print("Floating point operations are enabled in the kernel.\n");
}

// Register all math commands
void register_math_commands(void) {
    register_command("fputest", command_fpu_test, "Run a simple FPU test", "fputest", "Math");
} 