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
const MAX_BUFFER_SIZE: usize = 800 * 600; // VGA resolution - more reasonable size
static mut BUFFER_POOL: [[u32; MAX_BUFFER_SIZE]; 32] = [[0; MAX_BUFFER_SIZE]; 32];

// Track previous window positions for proper erasing
#[derive(Copy, Clone)]
struct WindowPosition {
    x: i32,
    y: i32,
    width: u32,
    height: u32,
}

// Dirty rectangle tracking for optimized rendering
#[derive(Copy, Clone)]
struct DirtyRect {
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
    mouse_x: i32,              // Current mouse X position for cursor rendering
    mouse_y: i32,              // Current mouse Y position for cursor rendering
    last_cursor_x: i32,        // Previous cursor X position (for clearing)
    last_cursor_y: i32,        // Previous cursor Y position (for clearing)
    cursor_backup: [u32; (12 + 2) * (16 + 2)], // Backup pixels under cursor (including outline)
    cursor_backup_valid: bool, // Whether cursor backup is valid
    dirty_rects: [Option<DirtyRect>; 32], // Track dirty regions for optimized rendering
    last_click_time: u64,      // Timestamp of last click for double-click detection
    last_click_x: i32,         // X position of last click
    last_click_y: i32,         // Y position of last click
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
            mouse_x: 0,
            mouse_y: 0,
            last_cursor_x: -1,
            last_cursor_y: -1,
            cursor_backup: [0; (12 + 2) * (16 + 2)],
            cursor_backup_valid: false,
            dirty_rects: [const { None }; 32],
            last_click_time: 0,
            last_click_x: 0,
            last_click_y: 0,
        }
    }

    fn get_framebuffer(&self) -> *mut LimineFramebuffer {
        self.framebuffer
    }

    // Bounds checking helper
    #[inline(always)]
    fn is_point_in_bounds(&self, x: i32, y: i32, fb: *mut LimineFramebuffer) -> bool {
        unsafe {
            x >= 0 && y >= 0 && 
            x < (*fb).width as i32 && 
            y < (*fb).height as i32
        }
    }

    // Mark a region as dirty for optimized rendering
    fn mark_dirty(&mut self, x: i32, y: i32, width: u32, height: u32) {
        // Find an empty slot or merge with existing dirty rects
        for i in 0..32 {
            if let Some(ref mut rect) = self.dirty_rects[i] {
                // Try to merge if overlapping or adjacent
                if x <= (rect.x + rect.width as i32) && (x + width as i32) >= rect.x &&
                    y <= (rect.y + rect.height as i32) && (y + height as i32) >= rect.y {
                    // Merge rectangles
                    let min_x = core::cmp::min(rect.x, x);
                    let min_y = core::cmp::min(rect.y, y);
                    let max_x = core::cmp::max(rect.x + rect.width as i32, x + width as i32);
                    let max_y = core::cmp::max(rect.y + rect.height as i32, y + height as i32);
                    rect.x = min_x;
                    rect.y = min_y;
                    rect.width = (max_x - min_x) as u32;
                    rect.height = (max_y - min_y) as u32;
                    return;
                }
            } else {
                // Empty slot, use it
                self.dirty_rects[i] = Some(DirtyRect { x, y, width, height });
                return;
            }
        }
    }

    // Bring window to front (top of Z-order)
    fn bring_to_front(&mut self, window: *mut Window) {
        // Find window index
        let mut idx = None;
        for i in 0..self.window_count {
            if self.windows[i] == Some(window) {
                idx = Some(i);
                break;
            }
        }
        
        if let Some(i) = idx {
            // Move window to end of array (top of Z-order)
            let win = self.windows[i];
            let prev_pos = self.previous_positions[i];
            
            for j in i..self.window_count - 1 {
                self.windows[j] = self.windows[j + 1];
                self.previous_positions[j] = self.previous_positions[j + 1];
            }
            
            self.windows[self.window_count - 1] = win;
            self.previous_positions[self.window_count - 1] = prev_pos;
            
            unsafe {
                if let Some(w) = win {
                    (*w).invalidated = true;
                }
            }
        }
    }

    // Resize window support
    fn resize_window(&mut self, window: *mut Window, new_width: u32, new_height: u32) {
        unsafe {
            // Validate new size
            let buffer_size = (new_width * new_height) as usize;
            if buffer_size > MAX_BUFFER_SIZE {
                return; // Too large
            }
            
            // Find window slot
            let slot = (*window).id as usize;
            if slot >= 32 {
                return;
            }
            
            // Clear old buffer area
            let old_size = ((*window).width * (*window).height) as usize;
            for i in 0..old_size {
                *(*window).buffer.add(i) = 0;
            }
            
            // Update dimensions
            (*window).width = new_width;
            (*window).height = new_height;
            (*window).invalidated = true;
            
            // Clear new size
            let new_size = buffer_size;
            for i in 0..new_size {
                *(*window).buffer.add(i) = 0;
            }
        }
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
        // Store mouse position for cursor rendering
        self.mouse_x = mouse_x;
        self.mouse_y = mouse_y;
        
        // Track button state for press detection
        let button_just_pressed = left_button && !self.last_mouse_button;
        
        // Double-click detection
        const DOUBLE_CLICK_TIME_MS: u64 = 500;
        const DOUBLE_CLICK_DISTANCE: i32 = 5;
        
        if button_just_pressed {
            // Get current timestamp (simplified - in real OS you'd use a timer)
            // For now, we'll use a simple counter that increments
            let current_time = self.last_click_time.wrapping_add(1);
            
            // Check for double-click
            let time_diff = current_time.saturating_sub(self.last_click_time);
            let dx = mouse_x - self.last_click_x;
            let dy = mouse_y - self.last_click_y;
            // Use squared distance comparison to avoid sqrt (not available in no_std)
            let distance_sq = dx * dx + dy * dy;
            let max_distance_sq = DOUBLE_CLICK_DISTANCE * DOUBLE_CLICK_DISTANCE;
            
            if time_diff < DOUBLE_CLICK_TIME_MS && distance_sq < max_distance_sq {
                // Double-click detected - could trigger maximize or other action
                // For now, just bring window to front
                if let Some(window) = self.focused_window {
                    self.bring_to_front(window);
                }
            }
            
            self.last_click_time = current_time;
            self.last_click_x = mouse_x;
            self.last_click_y = mouse_y;
        }
        
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
                                // Focus this window and bring to front
                                if let Some(old_focused) = self.focused_window {
                                    if old_focused != window {
                                        (*old_focused).focused = false;
                                        (*old_focused).invalidated = true;
                                    }
                                }
                                self.focused_window = Some(window);
                                (*window).focused = true;
                                (*window).invalidated = true;
                                self.bring_to_front(window);
                                
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
            if self.window_count > 0 {
                // Optimized: Direct rendering - much faster than double buffering
                // Clear desktop background only once when first window is created
                if !self.desktop_cleared {
                    self.clear_desktop_fast(fb);
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
            
            // Always render cursor last to ensure it's on top (even when no windows)
            self.render_cursor(fb);
        }
    }
    
    fn clear_cursor(&mut self, fb: *mut LimineFramebuffer, x: i32, y: i32) {
        unsafe {
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            const BACKUP_WIDTH: usize = CURSOR_WIDTH + 2;  // Include outline
            const BACKUP_HEIGHT: usize = CURSOR_HEIGHT + 2; // Include outline
            
            if !self.cursor_backup_valid {
                // If no backup available, just clear to desktop color (fallback)
                let fb_ptr = (*fb).address;
                let pitch = (*fb).pitch as usize / 4;
                let fb_w = (*fb).width as usize;
                let fb_h = (*fb).height as usize;
                
                for row in -1..=(CURSOR_HEIGHT as i32) {
                    for col in -1..=(CURSOR_WIDTH as i32) {
                        let px = x + col;
                        let py = y + row;
                        
                        if px >= 0 && py >= 0 && 
                           px < fb_w as i32 && py < fb_h as i32 {
                            *fb_ptr.add((py as usize) * pitch + (px as usize)) = self.desktop_color;
                        }
                    }
                }
                return;
            }
            
            // Restore saved background pixels
            let fb_ptr = (*fb).address;
            let pitch = (*fb).pitch as usize / 4;
            let fb_w = (*fb).width as usize;
            let fb_h = (*fb).height as usize;
            
            for row in 0..BACKUP_HEIGHT {
                for col in 0..BACKUP_WIDTH {
                    let px = x + col as i32 - 1; // -1 for outline
                    let py = y + row as i32 - 1; // -1 for outline
                    
                    if px >= 0 && py >= 0 && 
                       px < fb_w as i32 && py < fb_h as i32 {
                        *fb_ptr.add((py as usize) * pitch + (px as usize)) = 
                            self.cursor_backup[row * BACKUP_WIDTH + col];
                    }
                }
            }
            
            self.cursor_backup_valid = false;
        }
    }
    
    fn save_cursor_background(&mut self, fb: *mut LimineFramebuffer, x: i32, y: i32) {
        unsafe {
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            const BACKUP_WIDTH: usize = CURSOR_WIDTH + 2;  // Include outline
            const BACKUP_HEIGHT: usize = CURSOR_HEIGHT + 2; // Include outline
            
            let fb_ptr = (*fb).address;
            let pitch = (*fb).pitch as usize / 4;
            let fb_w = (*fb).width as usize;
            let fb_h = (*fb).height as usize;
            
            for row in 0..BACKUP_HEIGHT {
                for col in 0..BACKUP_WIDTH {
                    let px = x + col as i32 - 1; // -1 for outline
                    let py = y + row as i32 - 1; // -1 for outline
                    
                    if px >= 0 && py >= 0 && 
                       px < fb_w as i32 && py < fb_h as i32 {
                        self.cursor_backup[row * BACKUP_WIDTH + col] = 
                            *fb_ptr.add((py as usize) * pitch + (px as usize));
                    } else {
                        // If outside screen bounds, save desktop color
                        self.cursor_backup[row * BACKUP_WIDTH + col] = self.desktop_color;
                    }
                }
            }
            self.cursor_backup_valid = true;
        }
    }
    
    fn render_cursor(&mut self, fb: *mut LimineFramebuffer) {
        unsafe {
            // Mouse cursor bitmap (12x16 pixels)
            const CURSOR_BITMAP: [u16; 16] = [
                0b110000000000,  // ##
                0b111000000000,  // ###
                0b111100000000,  // ####
                0b111110000000,  // #####
                0b111111000000,  // ######
                0b111111100000,  // #######
                0b111111110000,  // ########
                0b111111111000,  // #########
                0b111111100000,  // #######
                0b111111100000,  // #######
                0b110110000000,  // ## ##
                0b110011000000,  // ##  ##
                0b100001100000,  // #    ##
                0b000001100000,  //      ##
                0b000000110000,  //       ##
                0b000000110000   //       ##
            ];
            
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            const CURSOR_COLOR: u32 = 0xFFFFFF; // White
            const CURSOR_OUTLINE_COLOR: u32 = 0x000000; // Black
            
            let x = self.mouse_x;
            let y = self.mouse_y;
            
            // Clear previous cursor position only if cursor actually moved
            if self.last_cursor_x >= 0 && self.last_cursor_y >= 0 && 
               (self.last_cursor_x != x || self.last_cursor_y != y) {
                self.clear_cursor(fb, self.last_cursor_x, self.last_cursor_y);
            }
            
            // Draw cursor at new position (only if position changed or first time)
            if self.last_cursor_x != x || self.last_cursor_y != y || 
               self.last_cursor_x == -1 || self.last_cursor_y == -1 {
                // Save background pixels before drawing
                self.save_cursor_background(fb, x, y);
                
                let fb_ptr = (*fb).address;
                let pitch = (*fb).pitch as usize / 4;
                let fb_w = (*fb).width as usize;
                let fb_h = (*fb).height as usize;
                
                // Draw cursor with black outline first, then white fill
                for row in 0..CURSOR_HEIGHT {
                    let bitmap_row = CURSOR_BITMAP[row];
                    for col in 0..CURSOR_WIDTH {
                        if (bitmap_row & (1 << (11 - col))) != 0 {
                            // Draw black outline pixels around the white pixel
                            for dy in -1..=1 {
                                for dx in -1..=1 {
                                    if dx == 0 && dy == 0 {
                                        continue; // Skip center pixel
                                    }
                                    let px = x + col as i32 + dx;
                                    let py = y + row as i32 + dy;
                                    
                                    if px >= 0 && py >= 0 && 
                                       px < fb_w as i32 && py < fb_h as i32 {
                                        *fb_ptr.add((py as usize) * pitch + (px as usize)) = CURSOR_OUTLINE_COLOR;
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Draw white cursor pixels on top
                for row in 0..CURSOR_HEIGHT {
                    let bitmap_row = CURSOR_BITMAP[row];
                    for col in 0..CURSOR_WIDTH {
                        if (bitmap_row & (1 << (11 - col))) != 0 {
                            let px = x + col as i32;
                            let py = y + row as i32;
                            
                            if px >= 0 && py >= 0 && 
                               px < fb_w as i32 && py < fb_h as i32 {
                                *fb_ptr.add((py as usize) * pitch + (px as usize)) = CURSOR_COLOR;
                            }
                        }
                    }
                }
                
                // Update last position
                self.last_cursor_x = x;
                self.last_cursor_y = y;
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
            
            // Optimized: Use copy_nonoverlapping for row-by-row copy (faster than pixel-by-pixel)
            for y in 0..win_h {
                let fb_y = win_y + y;
                if fb_y >= fb_h {
                    break;
                }
                
                let visible_width = core::cmp::min(win_w, fb_w.saturating_sub(win_x));
                if visible_width == 0 {
                    continue;
                }
                
                let src = win_buffer.add(y * win_w);
                let dst = fb_ptr.add(fb_y * pitch + win_x);
                
                // Fast copy entire row at once
                core::ptr::copy_nonoverlapping(src, dst, visible_width);
            }
        }
    }

    fn update(&mut self) {
        // Check if cursor moved BEFORE any rendering operations
        let cursor_moved = self.mouse_x != self.last_cursor_x || 
                           self.mouse_y != self.last_cursor_y;
        
        let needs_window_render = unsafe {
            if self.window_count == 0 {
                false
            } else {
                self.dragging_window.is_some() || 
                self.windows.iter()
                    .take(self.window_count)
                    .filter_map(|&w| w)
                    .any(|w| (*w).invalidated)
            }
        };
        
        // Only render if something changed
        if needs_window_render || cursor_moved {
            if needs_window_render {
                self.render(); // This calls render_cursor at the end
            } else if cursor_moved {
                // Only cursor moved, render it alone
                unsafe {
                    let fb = self.get_framebuffer();
                    if !fb.is_null() {
                        self.render_cursor(fb);
                    }
                }
            }
        }
    }

    // Optimized desktop clear using faster fill methods
    fn clear_desktop_fast(&mut self, fb: *mut LimineFramebuffer) {
        unsafe {
            let fb_ptr = (*fb).address;
            let _width = (*fb).width as usize;
            let height = (*fb).height as usize;
            let pitch = (*fb).pitch as usize / 4;
            let color = self.desktop_color;
            
            // Fill entire framebuffer row by row using write_volatile
            // This is faster than pixel-by-pixel and prevents optimization
            for y in 0..height {
                let row_ptr = fb_ptr.add(y * pitch);
                for x in 0.._width {
                    core::ptr::write_volatile(row_ptr.add(x), color);
                }
            }
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

#[no_mangle]
pub extern "C" fn wm_resize_window(window: *mut Window, new_width: u32, new_height: u32) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.resize_window(window, new_width, new_height);
        }
    }
}

#[no_mangle]
pub extern "C" fn wm_bring_to_front(window: *mut Window) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.bring_to_front(window);
        }
    }
}
