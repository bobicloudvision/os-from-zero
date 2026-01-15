#![no_std]
#![no_main]

// Panic handler for bare metal
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

use core::ptr;
use core::ffi::{c_char, c_int, c_void};

// Limine framebuffer structure (must match C struct)
#[repr(C)]
pub struct LimineFramebuffer {
    pub address: *mut u32,
    pub width: u64,
    pub height: u64,
    pub pitch: u64,
    pub bpp: u16,
    pub memory_model: u8,
    pub red_mask_size: u8,
    pub red_mask_shift: u8,
    pub green_mask_size: u8,
    pub green_mask_shift: u8,
    pub blue_mask_size: u8,
    pub blue_mask_shift: u8,
    pub unused: [u8; 7],
    pub edid_size: u64,
    pub edid: *mut c_void,
    pub mode_count: u64,
    pub modes: *mut *mut c_void,
}

// Window flags
pub const WINDOW_MOVABLE: u32 = 0x01;
pub const WINDOW_CLOSABLE: u32 = 0x02;
pub const WINDOW_RESIZABLE: u32 = 0x04;

// Window structure
#[repr(C)]
pub struct Window {
    pub id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub title: [u8; 64],
    pub flags: u32,
    pub focused: bool,
    pub invalidated: bool,
    pub draw_callback: Option<extern "C" fn(*mut Window)>,
    pub user_data: *mut c_void,
    pub buffer: *mut u32,  // Window content buffer
}

// Window manager state
static mut WM_STATE: Option<WindowManager> = None;

// Window pool for static allocation (shared between create and destroy)
static mut WINDOW_POOL: [Option<Window>; 32] = [const { None }; 32];
const MAX_BUFFER_SIZE: usize = 1920 * 1080; // HD support
static mut BUFFER_POOL: [[u32; MAX_BUFFER_SIZE]; 32] = [[0; MAX_BUFFER_SIZE]; 32];

// Track previous window positions for proper erasing
#[derive(Copy, Clone)]
struct WindowPosition {
    x: i32,
    y: i32,
    width: u32,
    height: u32,
}

struct WindowManager {
    framebuffer: *mut LimineFramebuffer,
    windows: [Option<*mut Window>; 32],
    window_count: usize,
    focused_window: Option<*mut Window>,
    dragging_window: Option<*mut Window>,
    drag_offset_x: i32,
    drag_offset_y: i32,
    desktop_color: u32,
    last_mouse_button: bool,  // Track previous button state for click detection
    desktop_cleared: bool,    // Track if desktop has been cleared (only clear once)
    previous_positions: [Option<WindowPosition>; 32], // Track previous window positions
}

impl WindowManager {
    fn new(framebuffer: *mut LimineFramebuffer) -> Self {
        WindowManager {
            framebuffer,
            windows: [None; 32],
            window_count: 0,
            focused_window: None,
            dragging_window: None,
            drag_offset_x: 0,
            drag_offset_y: 0,
            desktop_color: 0x0d1117, // Match terminal background (dark gray)
            last_mouse_button: false,
            desktop_cleared: false,
            previous_positions: [const { None }; 32],
        }
    }

    fn get_framebuffer(&self) -> *mut LimineFramebuffer {
        self.framebuffer
    }

    fn create_window(&mut self, title: *const c_char, x: i32, y: i32, width: u32, height: u32, flags: u32) -> *mut Window {
        if self.window_count >= 32 {
            return ptr::null_mut();
        }

        // Allocate window structure
        let window = unsafe {
            // Find an empty slot
            let mut slot_idx = None;
            for i in 0..32 {
                if WINDOW_POOL[i].is_none() {
                    slot_idx = Some(i);
                    break;
                }
            }
            
            let slot = match slot_idx {
                Some(idx) => idx,
                None => return ptr::null_mut(), // No free slots
            };
            
            // Initialize window in the slot
            let mut new_window = Window {
                id: slot as u32, // Use slot index as ID for uniqueness
                x: x,
                y: y,
                width: width,
                height: height,
                title: [0; 64],
                flags: flags,
                focused: false,
                invalidated: true,
                draw_callback: None,
                user_data: ptr::null_mut(),
                buffer: ptr::null_mut(),
            };
            
            // Copy title
            if !title.is_null() {
                let mut i = 0;
                let title_bytes = title as *const u8;
                while i < 63 && *title_bytes.add(i) != 0 {
                    new_window.title[i] = *title_bytes.add(i);
                    i += 1;
                }
                new_window.title[i] = 0;
            } else {
                new_window.title[0] = 0;
            }
            
            // Allocate window buffer - each window gets its own buffer
            // Support HD resolutions: 1920x1080 = 2,073,600 pixels
            let buffer_size = (width * height) as usize;
            if buffer_size <= MAX_BUFFER_SIZE {
                new_window.buffer = BUFFER_POOL[slot].as_mut_ptr();
            } else {
                // Window too large, can't allocate buffer
                return ptr::null_mut();
            }
            
            WINDOW_POOL[slot] = Some(new_window);
            let window_ptr = WINDOW_POOL[slot].as_mut().unwrap() as *mut Window;
            
            // Force initial render
            (*window_ptr).invalidated = true;
            
            window_ptr
        };

        self.windows[self.window_count] = Some(window);
        // Initialize previous position
        unsafe {
            self.previous_positions[self.window_count] = Some(WindowPosition {
                x: (*window).x,
                y: (*window).y,
                width: (*window).width,
                height: (*window).height,
            });
        }
        self.window_count += 1;
        self.focused_window = Some(window);
        unsafe { 
            (*window).focused = true;
            (*window).invalidated = true; // Force initial render
        }

        window
    }

    fn destroy_window(&mut self, window: *mut Window) {
        unsafe {
            let fb = self.get_framebuffer();
            if !fb.is_null() {
                // Erase window area from framebuffer
                // Use the exact window bounds to ensure complete erasure
                self.erase_window_area(fb, (*window).x, (*window).y, (*window).width, (*window).height);
            }
        }
        
        // Find and free the window slot in WINDOW_POOL using window ID (which is the slot index)
        unsafe {
            let window_id = (*window).id as usize;
            if window_id < 32 {
                WINDOW_POOL[window_id] = None;
            }
        }
        
        for i in 0..self.window_count {
            if let Some(w) = self.windows[i] {
                if w == window {
                    // Remove from array
                    for j in i..self.window_count - 1 {
                        self.windows[j] = self.windows[j + 1];
                        // Also shift previous positions
                        self.previous_positions[j] = self.previous_positions[j + 1];
                    }
                    self.windows[self.window_count - 1] = None;
                    self.previous_positions[self.window_count - 1] = None;
                    self.window_count -= 1;
                    
                    if self.focused_window == Some(window) {
                        self.focused_window = None;
                    }
                    if self.dragging_window == Some(window) {
                        self.dragging_window = None;
                    }
                    
                    // Invalidate all remaining windows to force a full re-render
                    // This ensures windows that were behind the destroyed window are properly redrawn
                    for j in 0..self.window_count {
                        if let Some(remaining_window) = self.windows[j] {
                            unsafe {
                                (*remaining_window).invalidated = true;
                            }
                        }
                    }
                    
                    // Force immediate render to clear any artifacts
                    if self.window_count > 0 {
                        self.render();
                    }
                    break;
                }
            }
        }
    }

    fn invalidate_window(&mut self, window: *mut Window) {
        unsafe {
            (*window).invalidated = true;
        }
    }

    fn clear_window(&mut self, window: *mut Window, color: u32) {
        unsafe {
            if (*window).buffer.is_null() {
                return;
            }
            
            let buffer = (*window).buffer;
            let size = ((*window).width * (*window).height) as usize;
            for i in 0..size {
                *buffer.add(i) = color;
            }
            // Don't set invalidated here - we're clearing as part of rendering
        }
    }

    fn draw_pixel_to_window(&mut self, window: *mut Window, x: i32, y: i32, color: u32) {
        unsafe {
            if (*window).buffer.is_null() {
                return;
            }
            
            if x < 0 || y < 0 || x >= (*window).width as i32 || y >= (*window).height as i32 {
                return;
            }
            
            let buffer = (*window).buffer;
            let index = (y as u32 * (*window).width + x as u32) as usize;
            *buffer.add(index) = color;
            // Don't set invalidated - we'll render when update is called
        }
    }

    fn draw_filled_rect_to_window(&mut self, window: *mut Window, x: i32, y: i32, width: u32, height: u32, color: u32) {
        unsafe {
            if (*window).buffer.is_null() {
                return;
            }
            
            let buffer = (*window).buffer;
            let win_width = (*window).width;
            
            for dy in 0..height {
                for dx in 0..width {
                    let px = x + dx as i32;
                    let py = y + dy as i32;
                    
                    if px >= 0 && py >= 0 && px < (*window).width as i32 && py < (*window).height as i32 {
                        let index = (py as u32 * win_width + px as u32) as usize;
                        *buffer.add(index) = color;
                    }
                }
            }
            // Don't set invalidated - we'll render when update is called
        }
    }

    fn draw_rect_to_window(&mut self, window: *mut Window, x: i32, y: i32, width: u32, height: u32, color: u32) {
        // Top and bottom lines
        self.draw_filled_rect_to_window(window, x, y, width, 1, color);
        self.draw_filled_rect_to_window(window, x, y + height as i32 - 1, width, 1, color);
        
        // Left and right lines
        self.draw_filled_rect_to_window(window, x, y, 1, height, color);
        self.draw_filled_rect_to_window(window, x + width as i32 - 1, y, 1, height, color);
    }

    fn draw_text_to_window(&mut self, window: *mut Window, text: *const c_char, x: i32, y: i32, color: u32) {
        unsafe {
            if (*window).buffer.is_null() {
                return;
            }
            
            if text.is_null() {
                return;
            }
            
            let mut current_x = x;
            let text_bytes = text as *const u8;
            let mut i = 0;
            const MAX_TEXT_LENGTH: usize = 1024; // Prevent infinite loops from unterminated strings
            
            while i < MAX_TEXT_LENGTH && *text_bytes.add(i) != 0 {
                let ch = *text_bytes.add(i) as usize;
                if ch >= 32 && ch <= 126 {
                    draw_char_to_window(window, ch as u8, current_x, y, color);
                    current_x += 8; // 8 pixels per character
                } else if ch == b'\n' as usize {
                    current_x = x;
                    // Note: newline handling would need y increment, simplified here
                }
                i += 1;
            }
            // Don't set invalidated - we'll render when update is called
        }
    }

    fn handle_mouse(&mut self, mouse_x: i32, mouse_y: i32, left_button: bool) {
        // Track button state for press detection
        let button_just_pressed = left_button && !self.last_mouse_button;
        self.last_mouse_button = left_button;
        
        // Check if we're dragging - continue dragging while button is held
        if let Some(dragging) = self.dragging_window {
            if left_button {
                unsafe {
                    let old_x = (*dragging).x;
                    let old_y = (*dragging).y;
                    
                    (*dragging).x = mouse_x - self.drag_offset_x;
                    (*dragging).y = mouse_y - self.drag_offset_y;
                    
                    // Clamp to screen bounds
                    let fb = self.get_framebuffer();
                    if (*dragging).x < 0 {
                        (*dragging).x = 0;
                    }
                    if (*dragging).y < 0 {
                        (*dragging).y = 0;
                    }
                    if (*dragging).x + (*dragging).width as i32 > (*fb).width as i32 {
                        (*dragging).x = (*fb).width as i32 - (*dragging).width as i32;
                    }
                    if (*dragging).y + (*dragging).height as i32 > (*fb).height as i32 {
                        (*dragging).y = (*fb).height as i32 - (*dragging).height as i32;
                    }
                    
                    // Invalidate if position changed
                    if old_x != (*dragging).x || old_y != (*dragging).y {
                        (*dragging).invalidated = true;
                    }
                }
            } else {
                // Button released - stop dragging
                self.dragging_window = None;
            }
            return;
        }

        // Check for window focus and drag start
        // Only check on button press (transition from not pressed to pressed)
        if button_just_pressed {
            // Check windows in reverse order (top to bottom)
            for i in (0..self.window_count).rev() {
                if let Some(window) = self.windows[i] {
                    unsafe {
                        let wx = (*window).x;
                        let wy = (*window).y;
                        let ww = (*window).width as i32;
                        let wh = (*window).height as i32;
                        
                        // Check if click is within window bounds
                        // Note: mouse coordinates are in screen space, window coordinates are in screen space
                        if mouse_x >= wx && mouse_x < wx + ww &&
                           mouse_y >= wy && mouse_y < wy + wh {
                            
                            // Check if click is on close button FIRST (before checking title bar)
                            // Close button takes priority over dragging
                            if ((*window).flags & WINDOW_CLOSABLE) != 0 {
                                let close_x_start = wx + (*window).width as i32 - 18;
                                let close_x_end = close_x_start + 16;
                                let close_y_start = wy + 2;
                                let close_y_end = close_y_start + 16;
                                
                                if mouse_x >= close_x_start && mouse_x < close_x_end &&
                                   mouse_y >= close_y_start && mouse_y < close_y_end {
                                    // Close button clicked
                                    self.destroy_window(window);
                                    return;
                                }
                            }
                            
                            // Check if click is in title bar (top 20 pixels, relative to window)
                            let title_bar_y_start = wy;
                            let title_bar_y_end = wy + 20;
                            
                            if mouse_y >= title_bar_y_start && mouse_y < title_bar_y_end {
                                // Focus this window
                                if let Some(old_focused) = self.focused_window {
                                    if old_focused != window {
                                        (*old_focused).focused = false;
                                        (*old_focused).invalidated = true;
                                    }
                                }
                                self.focused_window = Some(window);
                                (*window).focused = true;
                                (*window).invalidated = true;
                                
                                // Start dragging if window is movable
                                if ((*window).flags & WINDOW_MOVABLE) != 0 {
                                    self.dragging_window = Some(window);
                                    // Calculate offset from window origin (where in title bar we clicked)
                                    self.drag_offset_x = mouse_x - wx;
                                    self.drag_offset_y = mouse_y - wy;
                                }
                            }
                            break; // Stop checking other windows (topmost window gets the click)
                        }
                    }
                }
            }
        }
    }

    fn render(&mut self) {
        unsafe {
            let fb = self.get_framebuffer();
            if fb.is_null() {
                return;
            }
            
            // Only render desktop and windows if we have windows
            // Otherwise, let the terminal handle the display
            if self.window_count == 0 {
                return;
            }
            
            let width = (*fb).width as usize;
            let height = (*fb).height as usize;
            let pitch = (*fb).pitch as usize / 4;
            
            // Optimized: Direct rendering - much faster than double buffering
            // Clear desktop background only once when first window is created
            if !self.desktop_cleared {
                let fb_ptr = (*fb).address;
                for y in 0..height {
                    for x in 0..width {
                        *fb_ptr.add(y * pitch + x) = self.desktop_color;
                    }
                }
                self.desktop_cleared = true;
            }
            
            // Draw all windows (only invalidated windows will redraw their buffers)
            // This is optimized - we only update what changed
            for i in 0..self.window_count {
                if let Some(window) = self.windows[i] {
                    self.render_window(window, fb);
                }
            }
        }
    }
    

    fn erase_window_area(&mut self, fb: *mut LimineFramebuffer, x: i32, y: i32, width: u32, height: u32) {
        unsafe {
            let fb_ptr = (*fb).address;
            let pitch = (*fb).pitch as usize / 4;
            let fb_w = (*fb).width as usize;
            let fb_h = (*fb).height as usize;
            
            // Calculate bounds with proper clamping and overflow protection
            let start_x = if x < 0 { 0 } else { x as usize };
            let start_y = if y < 0 { 0 } else { y as usize };
            let end_x = {
                // Check for overflow before casting
                let x_i64 = x as i64;
                let width_i64 = width as i64;
                if x_i64 > 0 && width_i64 > 0 && x_i64 > (usize::MAX as i64) - width_i64 {
                    fb_w // Overflow protection: clamp to framebuffer width
                } else {
                    let calculated = (x_i64 + width_i64) as usize;
                    if calculated > fb_w { fb_w } else { calculated }
                }
            };
            let end_y = {
                // Check for overflow before casting
                let y_i64 = y as i64;
                let height_i64 = height as i64;
                if y_i64 > 0 && height_i64 > 0 && y_i64 > (usize::MAX as i64) - height_i64 {
                    fb_h // Overflow protection: clamp to framebuffer height
                } else {
                    let calculated = (y_i64 + height_i64) as usize;
                    if calculated > fb_h { fb_h } else { calculated }
                }
            };
            
            // Ensure we have valid bounds
            if start_x < end_x && start_y < end_y {
                for fy in start_y..end_y {
                    for fx in start_x..end_x {
                        *fb_ptr.add(fy * pitch + fx) = self.desktop_color;
                    }
                }
            }
        }
    }
    

    fn render_window(&mut self, window: *mut Window, fb: *mut LimineFramebuffer) {
        unsafe {
            if (*window).buffer.is_null() {
                return; // Can't render without a buffer
            }
            
            // Find window index to get previous position
            let mut window_idx = None;
            for i in 0..self.window_count {
                if self.windows[i] == Some(window) {
                    window_idx = Some(i);
                    break;
                }
            }
            
            let current_x = (*window).x;
            let current_y = (*window).y;
            let current_w = (*window).width;
            let current_h = (*window).height;
            
            // Erase old position if window moved
            if let Some(idx) = window_idx {
                if let Some(ref prev_pos) = self.previous_positions[idx] {
                    if prev_pos.x != current_x || prev_pos.y != current_y ||
                       prev_pos.width != current_w || prev_pos.height != current_h {
                        // Window moved or resized - erase old position
                        self.erase_window_area(fb, prev_pos.x, prev_pos.y, prev_pos.width, prev_pos.height);
                    }
                }
                // Update previous position
                self.previous_positions[idx] = Some(WindowPosition {
                    x: current_x,
                    y: current_y,
                    width: current_w,
                    height: current_h,
                });
            }
            
            if (*window).invalidated {
                // Clear window buffer
                self.clear_window(window, 0x2d2d2d); // Window background
                
                // Draw window border and title bar
                let title_color = if (*window).focused { 0x4a90e2 } else { 0x404040 };
                self.draw_filled_rect_to_window(window, 0, 0, (*window).width, 20, title_color);
                
                // Draw title text
                let title_ptr = (*window).title.as_ptr() as *const c_char;
                self.draw_text_to_window(window, title_ptr, 4, 4, 0xffffff);
                
                // Draw close button if closable
                if ((*window).flags & WINDOW_CLOSABLE) != 0 {
                    self.draw_filled_rect_to_window(window, (*window).width as i32 - 18, 2, 16, 16, 0xff4444);
                    // Draw 'X' character for close button
                    draw_char_to_window(window, b'X', (*window).width as i32 - 14, 4, 0xffffff);
                }
                
                // Call custom draw callback if set
                if let Some(callback) = (*window).draw_callback {
                    callback(window);
                }
                
                (*window).invalidated = false;
            }
            
            // Blit window buffer to framebuffer
            let fb_ptr = (*fb).address;
            let pitch = (*fb).pitch as usize / 4;
            let win_x = (*window).x as usize;
            let win_y = (*window).y as usize;
            let win_w = (*window).width as usize;
            let win_h = (*window).height as usize;
            let fb_w = (*fb).width as usize;
            let fb_h = (*fb).height as usize;
            
            if (*window).buffer.is_null() {
                return;
            }
            
            let win_buffer = (*window).buffer;
            
            for y in 0..win_h {
                let fb_y = win_y + y;
                if fb_y >= fb_h {
                    break;
                }
                
                for x in 0..win_w {
                    let fb_x = win_x + x;
                    if fb_x >= fb_w {
                        break;
                    }
                    
                    let pixel = *win_buffer.add(y * win_w + x);
                    *fb_ptr.add(fb_y * pitch + fb_x) = pixel;
                }
            }
        }
    }

    fn update(&mut self) {
        // Only render if we have windows
        if self.window_count == 0 {
            return;
        }
        
        // Optimized: Only render when something actually changed
        // Check if any window needs rendering (invalidated or being dragged)
        let needs_render = unsafe {
            // Always render when dragging (for smooth movement)
            if self.dragging_window.is_some() {
                true
            } else {
                // Check if any window is invalidated
                let mut needs = false;
                for i in 0..self.window_count {
                    if let Some(window) = self.windows[i] {
                        if (*window).invalidated {
                            needs = true;
                            break;
                        }
                    }
                }
                needs
            }
        };
        
        if needs_render {
            self.render();
        }
    }
}

// 8x8 font data - get glyph for a character
fn get_font_glyph(ch: u8) -> [u8; 8] {
    match ch {
        b' ' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
        b'0' => [0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00],
        b'1' => [0x18, 0x18, 0x38, 0x18, 0x18, 0x18, 0x7E, 0x00],
        b'2' => [0x3C, 0x66, 0x06, 0x0C, 0x30, 0x60, 0x7E, 0x00],
        b'3' => [0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00],
        b'4' => [0x06, 0x0E, 0x1E, 0x66, 0x7F, 0x06, 0x06, 0x00],
        b'5' => [0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00],
        b'6' => [0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00],
        b'7' => [0x7E, 0x66, 0x0C, 0x18, 0x18, 0x18, 0x18, 0x00],
        b'8' => [0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00],
        b'9' => [0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00],
        b'A' => [0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00],
        b'B' => [0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00],
        b'C' => [0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00],
        b'D' => [0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x7C, 0x00],
        b'E' => [0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00],
        b'F' => [0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00],
        b'G' => [0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00],
        b'H' => [0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00],
        b'I' => [0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00],
        b'J' => [0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00],
        b'K' => [0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00],
        b'L' => [0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00],
        b'M' => [0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00],
        b'N' => [0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00],
        b'O' => [0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00],
        b'P' => [0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00],
        b'Q' => [0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x0E, 0x00],
        b'R' => [0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00],
        b'S' => [0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00],
        b'T' => [0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00],
        b'U' => [0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00],
        b'V' => [0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00],
        b'W' => [0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00],
        b'X' => [0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00],
        b'Y' => [0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00],
        b'Z' => [0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00],
        b'a' => [0x00, 0x00, 0x3C, 0x06, 0x3E, 0x66, 0x3E, 0x00],
        b'b' => [0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00],
        b'c' => [0x00, 0x00, 0x3C, 0x60, 0x60, 0x60, 0x3C, 0x00],
        b'd' => [0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00],
        b'e' => [0x00, 0x00, 0x3C, 0x66, 0x7E, 0x60, 0x3C, 0x00],
        b'f' => [0x1C, 0x36, 0x30, 0x7C, 0x30, 0x30, 0x30, 0x00],
        b'g' => [0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x7C],
        b'h' => [0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00],
        b'i' => [0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x3C, 0x00],
        b'j' => [0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x66, 0x3C],
        b'k' => [0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00],
        b'l' => [0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00],
        b'm' => [0x00, 0x00, 0x66, 0x7F, 0x7F, 0x6B, 0x63, 0x00],
        b'n' => [0x00, 0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00],
        b'o' => [0x00, 0x00, 0x3C, 0x66, 0x66, 0x66, 0x3C, 0x00],
        b'p' => [0x00, 0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60],
        b'q' => [0x00, 0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06],
        b'r' => [0x00, 0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x00],
        b's' => [0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00],
        b't' => [0x18, 0x18, 0x7E, 0x18, 0x18, 0x18, 0x0E, 0x00],
        b'u' => [0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00],
        b'v' => [0x00, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00],
        b'w' => [0x00, 0x00, 0x63, 0x6B, 0x7F, 0x3E, 0x36, 0x00],
        b'x' => [0x00, 0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x00],
        b'y' => [0x00, 0x00, 0x66, 0x66, 0x66, 0x3E, 0x0C, 0x78],
        b'z' => [0x00, 0x00, 0x7E, 0x0C, 0x18, 0x30, 0x7E, 0x00],
        b'!' => [0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00],
        b':' => [0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00],
        b'(' => [0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00],
        b')' => [0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00],
        b'.' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00],
        b',' => [0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x30, 0x00],
        b'-' => [0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00],
        b'=' => [0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00],
        b'+' => [0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00],
        b'/' => [0x00, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x00, 0x00],
        b'\\' => [0x00, 0x60, 0x30, 0x18, 0x0C, 0x06, 0x00, 0x00],
        b'%' => [0x00, 0x46, 0x66, 0x30, 0x18, 0xCC, 0xC4, 0x00],
        _ => [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], // Unknown character
    }
}

fn draw_char_to_window(window: *mut Window, ch: u8, x: i32, y: i32, color: u32) {
    unsafe {
        if (*window).buffer.is_null() {
            return;
        }
        
        let glyph = get_font_glyph(ch);
        let buffer = (*window).buffer;
        let win_width = (*window).width;
        
        for row in 0..8 {
            for col in 0..8 {
                if (glyph[row] & (1 << (7 - col))) != 0 {
                    let px = x + col as i32;
                    let py = y + row as i32;
                    
                    if px >= 0 && py >= 0 && px < (*window).width as i32 && py < (*window).height as i32 {
                        let index = (py as u32 * win_width + px as u32) as usize;
                        *buffer.add(index) = color;
                    }
                }
            }
        }
    }
}

// FFI Functions

#[no_mangle]
pub extern "C" fn wm_init(framebuffer: *mut LimineFramebuffer) {
    unsafe {
        WM_STATE = Some(WindowManager::new(framebuffer));
    }
}

#[no_mangle]
pub extern "C" fn wm_create_window(
    title: *const c_char,
    x: c_int,
    y: c_int,
    width: u32,
    height: u32,
    flags: u32,
) -> *mut Window {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.create_window(title, x, y, width, height, flags)
        } else {
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_destroy_window(window: *mut Window) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.destroy_window(window);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_invalidate_window(window: *mut Window) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.invalidate_window(window);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_clear_window(window: *mut Window, color: u32) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.clear_window(window, color);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_draw_pixel_to_window(window: *mut Window, x: c_int, y: c_int, color: u32) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.draw_pixel_to_window(window, x, y, color);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_draw_filled_rect_to_window(
    window: *mut Window,
    x: c_int,
    y: c_int,
    width: u32,
    height: u32,
    color: u32,
) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.draw_filled_rect_to_window(window, x, y, width, height, color);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_draw_rect_to_window(
    window: *mut Window,
    x: c_int,
    y: c_int,
    width: u32,
    height: u32,
    color: u32,
) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.draw_rect_to_window(window, x, y, width, height, color);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_draw_text_to_window(
    window: *mut Window,
    text: *const c_char,
    x: c_int,
    y: c_int,
    color: u32,
) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.draw_text_to_window(window, text, x, y, color);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_handle_mouse(mouse_x: c_int, mouse_y: c_int, left_button: bool) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.handle_mouse(mouse_x, mouse_y, left_button);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_update() {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.update();
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_get_window_count() -> c_int {
    unsafe {
        if let Some(ref wm) = WM_STATE {
            wm.window_count as c_int
        } else {
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_get_window_info(index: c_int, x: *mut c_int, y: *mut c_int, 
                                     w: *mut c_int, h: *mut c_int, title: *mut c_char) {
    unsafe {
        if let Some(ref wm) = WM_STATE {
            if index >= 0 && (index as usize) < wm.window_count {
                if let Some(window) = wm.windows[index as usize] {
                    *x = (*window).x;
                    *y = (*window).y;
                    *w = (*window).width as c_int;
                    *h = (*window).height as c_int;
                    
                    // Copy title
                    let title_bytes = (*window).title.as_ptr();
                    let title_dest = title as *mut u8;
                    let mut i = 0;
                    while i < 63 && *title_bytes.add(i) != 0 {
                        *title_dest.add(i) = *title_bytes.add(i);
                        i += 1;
                    }
                    *title_dest.add(i) = 0;
                }
            }
        }
    }
}
