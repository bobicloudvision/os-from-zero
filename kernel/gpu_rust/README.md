# GPU Rendering Module

This module provides GPU-accelerated rendering functions for the OS, optimized for QEMU virtual machines.

## Features

- **Hardware-accelerated blitting**: Fast memory copy operations
- **Optimized fill operations**: Fast rectangle filling
- **Alpha blending**: Support for transparency
- **Command queue**: Foundation for future VirtIO GPU support

## Usage

The GPU module is automatically initialized when the OS boots. The window manager uses GPU acceleration automatically when available.

### Manual GPU Operations

```c
#include "gpu_rust.h"

// Check if GPU is available
if (gpu_is_available()) {
    // Use GPU-accelerated blitting
    gpu_blit(dst, dst_pitch, src, src_pitch, width, height);
    
    // Fill a rectangle
    gpu_fill_rect(buffer, pitch, x, y, width, height, color);
    
    // Clear entire framebuffer
    gpu_clear(buffer, width, height, color);
}
```

## QEMU Configuration

The QEMU configuration includes VirtIO GPU support:

```
-device virtio-gpu-pci
```

This enables hardware-accelerated rendering in QEMU.

## PCI Device Detection

The OS automatically enumerates PCI devices on boot, allowing detection of GPU hardware. The PCI module can find display devices:

```c
#include "pci.h"

// Find VGA device
struct pci_device *vga = pci_find_class(PCI_CLASS_DISPLAY, 0x00);

// Find any display device
struct pci_device *display = pci_find_class(PCI_CLASS_DISPLAY, 0);
```

## Future Enhancements

- Full VirtIO GPU protocol implementation
- 3D rendering support
- Hardware cursor support
- Multiple display support
