# Window Manager Examples

This file demonstrates how to use the Rust window manager from C code.

## Basic Usage

### Creating a Window

```c
#include "window_manager_rust.h"

window_t *win = wm_create_window("My Window", 100, 100, 300, 200, 
                                 WINDOW_MOVABLE | WINDOW_CLOSABLE);
```

Parameters:
- `title`: Window title (string)
- `x, y`: Window position
- `width, height`: Window size
- `flags`: Window flags (WINDOW_MOVABLE, WINDOW_CLOSABLE, WINDOW_RESIZABLE)

### Drawing to a Window

```c
// Clear window with a color
wm_clear_window(win, 0x2d2d2d);  // Dark gray

// Draw text
wm_draw_text_to_window(win, "Hello World!", 10, 30, 0xffffff);  // White text

// Draw filled rectangle
wm_draw_filled_rect_to_window(win, 10, 50, 100, 30, 0xff0000);  // Red rectangle

// Draw rectangle outline
wm_draw_rect_to_window(win, 10, 90, 100, 30, 0x00ff00);  // Green border

// Draw a pixel
wm_draw_pixel_to_window(win, 50, 120, 0x0000ff);  // Blue pixel
```

### Window Flags

- `WINDOW_MOVABLE`: Window can be dragged by the title bar
- `WINDOW_CLOSABLE`: Window has a close button
- `WINDOW_RESIZABLE`: Window can be resized (not yet implemented)

### Invalidating Windows

When you modify window content, mark it for redraw:

```c
wm_invalidate_window(win);
```

### Destroying Windows

```c
wm_destroy_window(win);
```

## Example Commands

Run these commands in the shell:

- `windows` - Create all example windows
- `windows simple` - Create a simple text window
- `windows colors` - Create a window with colored rectangles
- `windows pattern` - Create a window with a checkerboard pattern
- `windows info` - Create an informational window
- `windows multiple` - Create multiple windows at once

## Color Format

Colors are in RGB format (0xRRGGBB):
- `0xff0000` - Red
- `0x00ff00` - Green
- `0x0000ff` - Blue
- `0xffffff` - White
- `0x000000` - Black
- `0x2d2d2d` - Dark gray
- `0x4a90e2` - Blue (window title bar)

## Mouse Interaction

Windows automatically handle:
- Clicking on title bar to focus
- Dragging windows (if WINDOW_MOVABLE flag is set)
- Clicking close button (if WINDOW_CLOSABLE flag is set)

The window manager updates are called automatically from the shell's mouse event handler.
