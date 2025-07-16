// kernel/kernel64.c - Minimal 64-bit C kernel

void kernel_main(void) {
    volatile char *video = (volatile char*)0xb8000;
    const char *msg = "Hello from 64-bit kernel!";
    for (int i = 0; msg[i] != '\0'; ++i) {
        video[i * 2] = msg[i];
        video[i * 2 + 1] = 0x0F;
    }
    while (1) {}
} 