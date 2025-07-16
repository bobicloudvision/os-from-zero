 #ifndef FPU_SIMPLE_H
#define FPU_SIMPLE_H

#include <stdint.h>
#include <stdbool.h>

// FPU control word bits
#define FPU_CW_PRECISION_MASK   0x0300
#define FPU_CW_PRECISION_64     0x0300  // 64-bit precision
#define FPU_CW_PRECISION_53     0x0200  // 53-bit precision (default)
#define FPU_CW_PRECISION_24     0x0000  // 24-bit precision

#define FPU_CW_ROUNDING_MASK    0x0C00
#define FPU_CW_ROUNDING_NEAREST 0x0000  // Round to nearest (default)
#define FPU_CW_ROUNDING_DOWN    0x0400  // Round down
#define FPU_CW_ROUNDING_UP      0x0800  // Round up
#define FPU_CW_ROUNDING_ZERO    0x0C00  // Round toward zero

#define FPU_CW_EXCEPTION_MASK   0x003F
#define FPU_CW_MASK_INVALID     0x0001  // Invalid operation
#define FPU_CW_MASK_DENORM      0x0002  // Denormalized operand
#define FPU_CW_MASK_DIVZERO     0x0004  // Division by zero
#define FPU_CW_MASK_OVERFLOW    0x0008  // Overflow
#define FPU_CW_MASK_UNDERFLOW   0x0010  // Underflow
#define FPU_CW_MASK_PRECISION   0x0020  // Precision

// MXCSR control bits for SSE
#define MXCSR_EXCEPTION_MASK    0x1F80
#define MXCSR_MASK_INVALID      0x0080
#define MXCSR_MASK_DENORM       0x0100
#define MXCSR_MASK_DIVZERO      0x0200
#define MXCSR_MASK_OVERFLOW     0x0400
#define MXCSR_MASK_UNDERFLOW    0x0800
#define MXCSR_MASK_PRECISION    0x1000

#define MXCSR_ROUNDING_MASK     0x6000
#define MXCSR_ROUNDING_NEAREST  0x0000
#define MXCSR_ROUNDING_DOWN     0x2000
#define MXCSR_ROUNDING_UP       0x4000
#define MXCSR_ROUNDING_ZERO     0x6000

#define MXCSR_FLUSH_ZERO        0x8000
#define MXCSR_DENORM_ZERO       0x0040

// Function declarations
bool fpu_init(void);
void fpu_enable(void);
void fpu_disable(void);
bool fpu_is_enabled(void);

// FPU state management
void fpu_save_state(void *buffer);
void fpu_restore_state(const void *buffer);
void fpu_init_state(void);

// Control word operations
uint16_t fpu_get_control_word(void);
void fpu_set_control_word(uint16_t cw);

// SSE operations
bool sse_is_supported(void);
void sse_enable(void);
uint32_t sse_get_mxcsr(void);
void sse_set_mxcsr(uint32_t mxcsr);

// Utility functions
void fpu_clear_exceptions(void);
uint16_t fpu_get_status_word(void);
bool fpu_has_exception(void);

// Math functions
float math_sqrt(float x);
float math_sin(float x);
float math_cos(float x);

#endif // FPU_SIMPLE_H 