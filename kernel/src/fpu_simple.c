 #include "fpu_simple.h"

// Check if CPUID is supported
static bool cpuid_supported(void) {
    uint32_t eax, ebx, ecx, edx;
    
    // Try to flip the ID bit in EFLAGS
    __asm__ volatile (
        "pushfq\n\t"
        "popq %%rax\n\t"
        "movq %%rax, %%rbx\n\t"
        "xorq $0x200000, %%rax\n\t"
        "pushq %%rax\n\t"
        "popfq\n\t"
        "pushfq\n\t"
        "popq %%rax\n\t"
        "cmpq %%rax, %%rbx\n\t"
        "setne %%al\n\t"
        "movzbl %%al, %%eax"
        : "=a" (eax)
        :
        : "rbx", "cc"
    );
    
    return eax != 0;
}

// Execute CPUID instruction
static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile (
        "cpuid"
        : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
        : "a" (leaf)
    );
}

// Check if FPU is present
static bool fpu_present(void) {
    if (!cpuid_supported()) {
        return false;
    }
    
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    
    // Check FPU bit (bit 0 of EDX)
    return (edx & 0x1) != 0;
}

// Check if SSE is supported
bool sse_is_supported(void) {
    if (!cpuid_supported()) {
        return false;
    }
    
    uint32_t eax, ebx, ecx, edx;
    cpuid(1, &eax, &ebx, &ecx, &edx);
    
    // Check SSE bit (bit 25 of EDX)
    return (edx & (1 << 25)) != 0;
}

// Initialize FPU
bool fpu_init(void) {
    if (!fpu_present()) {
        return false;
    }
    
    // Enable FPU in CR0
    uint64_t cr0;
    __asm__ volatile ("movq %%cr0, %0" : "=r" (cr0));
    
    // Clear EM (bit 2) and TS (bit 3) bits
    cr0 &= ~(1 << 2);  // Clear EM (FPU emulation)
    cr0 &= ~(1 << 3);  // Clear TS (task switch)
    
    // Set MP (bit 1) for monitoring coprocessor
    cr0 |= (1 << 1);
    
    __asm__ volatile ("movq %0, %%cr0" :: "r" (cr0));
    
    // Initialize FPU state
    fpu_init_state();
    
    // Set up control word for kernel use
    uint16_t cw = FPU_CW_PRECISION_64 | FPU_CW_ROUNDING_NEAREST | FPU_CW_EXCEPTION_MASK;
    fpu_set_control_word(cw);
    
    // Enable SSE if supported
    if (sse_is_supported()) {
        sse_enable();
    }
    
    return true;
}

// Enable FPU
void fpu_enable(void) {
    uint64_t cr0;
    __asm__ volatile ("movq %%cr0, %0" : "=r" (cr0));
    
    // Clear EM and TS bits
    cr0 &= ~(1 << 2);
    cr0 &= ~(1 << 3);
    
    __asm__ volatile ("movq %0, %%cr0" :: "r" (cr0));
}

// Disable FPU
void fpu_disable(void) {
    uint64_t cr0;
    __asm__ volatile ("movq %%cr0, %0" : "=r" (cr0));
    
    // Set EM bit
    cr0 |= (1 << 2);
    
    __asm__ volatile ("movq %0, %%cr0" :: "r" (cr0));
}

// Check if FPU is enabled
bool fpu_is_enabled(void) {
    uint64_t cr0;
    __asm__ volatile ("movq %%cr0, %0" : "=r" (cr0));
    
    // Check if EM bit is clear
    return (cr0 & (1 << 2)) == 0;
}

// Enable SSE
void sse_enable(void) {
    if (!sse_is_supported()) {
        return;
    }
    
    uint64_t cr4;
    __asm__ volatile ("movq %%cr4, %0" : "=r" (cr4));
    
    // Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10) bits
    cr4 |= (1 << 9);   // OSFXSR: OS supports FXSAVE/FXRSTOR
    cr4 |= (1 << 10);  // OSXMMEXCPT: OS supports SSE exceptions
    
    __asm__ volatile ("movq %0, %%cr4" :: "r" (cr4));
    
    // Set up MXCSR for kernel use
    uint32_t mxcsr = MXCSR_ROUNDING_NEAREST | MXCSR_EXCEPTION_MASK;
    sse_set_mxcsr(mxcsr);
}

// Initialize FPU state
void fpu_init_state(void) {
    __asm__ volatile ("fninit");
}

// Save FPU state
void fpu_save_state(void *buffer) {
    if (sse_is_supported()) {
        __asm__ volatile ("fxsave (%0)" :: "r" (buffer) : "memory");
    } else {
        __asm__ volatile ("fsave (%0)" :: "r" (buffer) : "memory");
    }
}

// Restore FPU state
void fpu_restore_state(const void *buffer) {
    if (sse_is_supported()) {
        __asm__ volatile ("fxrstor (%0)" :: "r" (buffer) : "memory");
    } else {
        __asm__ volatile ("frstor (%0)" :: "r" (buffer) : "memory");
    }
}

// Get FPU control word
uint16_t fpu_get_control_word(void) {
    uint16_t cw;
    __asm__ volatile ("fnstcw %0" : "=m" (cw));
    return cw;
}

// Set FPU control word
void fpu_set_control_word(uint16_t cw) {
    __asm__ volatile ("fldcw %0" :: "m" (cw));
}

// Get FPU status word
uint16_t fpu_get_status_word(void) {
    uint16_t sw;
    __asm__ volatile ("fnstsw %0" : "=m" (sw));
    return sw;
}

// Clear FPU exceptions
void fpu_clear_exceptions(void) {
    __asm__ volatile ("fnclex");
}

// Check if FPU has exception
bool fpu_has_exception(void) {
    uint16_t sw = fpu_get_status_word();
    return (sw & 0x3F) != 0;  // Check exception bits
}

// Get SSE MXCSR register
uint32_t sse_get_mxcsr(void) {
    uint32_t mxcsr;
    __asm__ volatile ("stmxcsr %0" : "=m" (mxcsr));
    return mxcsr;
}

// Set SSE MXCSR register
void sse_set_mxcsr(uint32_t mxcsr) {
    __asm__ volatile ("ldmxcsr %0" :: "m" (mxcsr));
}

// Math functions using x87 FPU
float math_sqrt(float x) {
    float result;
    __asm__ volatile (
        "flds %1\n\t"
        "fsqrt\n\t"
        "fstps %0"
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

float math_sin(float x) {
    float result;
    __asm__ volatile (
        "flds %1\n\t"
        "fsin\n\t"
        "fstps %0"
        : "=m" (result)
        : "m" (x)
    );
    return result;
}

float math_cos(float x) {
    float result;
    __asm__ volatile (
        "flds %1\n\t"
        "fcos\n\t"
        "fstps %0"
        : "=m" (result)
        : "m" (x)
    );
    return result;
} 