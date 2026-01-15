#![no_std]
#![no_main]

// Panic handler for bare metal
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

use core::ptr;
use core::ffi::{c_char, c_int, c_void};

// Helper function for logging (simplified - just pass static strings)
#[inline(always)]
unsafe fn log_debug(msg: &[u8]) {
    let mut buf = [0u8; 256];
    let mut i = 0;
    for &b in msg.iter().take(255) {
        buf[i] = b;
        i += 1;
        if b == 0 { break; }
    }
    if i < 255 { buf[i] = 0; }
    logger_rust_log(0, b"WM\0".as_ptr() as *const c_char, buf.as_ptr() as *const c_char);
}

#[inline(always)]
unsafe fn log_info(msg: &[u8]) {
    let mut buf = [0u8; 256];
    let mut i = 0;
    for &b in msg.iter().take(255) {
        buf[i] = b;
        i += 1;
        if b == 0 { break; }
    }
    if i < 255 { buf[i] = 0; }
    logger_rust_log(1, b"WM\0".as_ptr() as *const c_char, buf.as_ptr() as *const c_char);
}

#[inline(always)]
unsafe fn log_error(msg: &[u8]) {
    let mut buf = [0u8; 256];
    let mut i = 0;
    for &b in msg.iter().take(255) {
        buf[i] = b;
        i += 1;
        if b == 0 { break; }
    }
    if i < 255 { buf[i] = 0; }
    logger_rust_log(3, b"WM\0".as_ptr() as *const c_char, buf.as_ptr() as *const c_char);
}

// Surface structure (must match display server definition)
#[repr(C)]
pub struct Surface {
    pub id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub buffer: *mut u32,
    pub z_order: i32,
}

// External display server functions
extern "C" {
    fn ds_create_surface(x: c_int, y: c_int, width: u32, height: u32, z_order: c_int) -> *mut Surface;
    fn ds_destroy_surface(surface: *mut Surface);
    fn ds_set_surface_position(surface: *mut Surface, x: c_int, y: c_int);
    fn ds_set_surface_z_order(surface: *mut Surface, z_order: c_int);
    fn ds_set_surface_size(surface: *mut Surface, width: u32, height: u32);
    fn ds_get_surface_buffer(surface: *mut Surface) -> *mut u32;
    fn ds_mark_dirty(x: c_int, y: c_int, width: u32, height: u32);
    fn ds_update_cursor_position(x: c_int, y: c_int);
    fn ds_render();
}

// External logger functions
extern "C" {
    fn logger_rust_log(level: u32, module: *const c_char, message: *const c_char);
    fn logger_rust_log_fmt(level: u32, module: *const c_char, format: *const c_char, ...);
}

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
    pub buffer: *mut u32,  // Window content buffer (points to display server surface buffer)
    pub surface: *mut Surface, // Display server surface handle
    pub z_order: i32,      // Z-order for compositing
    pub minimized: bool,   // Whether window is minimized
    pub maximized: bool,   // Whether window is maximized
    pub orig_x: i32,       // Original x position before maximize
    pub orig_y: i32,       // Original y position before maximize
    pub orig_width: u32,   // Original width before maximize/resize
    pub orig_height: u32,  // Original height before maximize/resize
}

// Window manager state
static mut WM_STATE: Option<WindowManager> = None;

// Window pool for static allocation
static mut WINDOW_POOL: [Option<Window>; 32] = [const { None }; 32];

// Resize edge types
#[derive(Clone, Copy, PartialEq)]
enum ResizeEdge {
    None,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
}

struct WindowManager {
    framebuffer: *mut LimineFramebuffer,
    windows: [Option<*mut Window>; 32],
    window_count: usize,
    focused_window: Option<*mut Window>,
    dragging_window: Option<*mut Window>,
    drag_offset_x: i32,
    drag_offset_y: i32,
    resizing_window: Option<*mut Window>,
    resize_edge: ResizeEdge,
    resize_start_x: i32,
    resize_start_y: i32,
    resize_start_width: u32,
    resize_start_height: u32,
    last_mouse_button: bool,
    next_z_order: i32,  // Next z-order value to assign
    min_z_order: i32,   // Minimum z-order for minimized windows
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
            resizing_window: None,
            resize_edge: ResizeEdge::None,
            resize_start_x: 0,
            resize_start_y: 0,
            resize_start_width: 0,
            resize_start_height: 0,
            last_mouse_button: false,
            next_z_order: 0,
            min_z_order: -1000, // Z-order for minimized windows
        }
    }

    fn get_framebuffer(&self) -> *mut LimineFramebuffer {
        self.framebuffer
    }

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
            
            for j in i..self.window_count - 1 {
                self.windows[j] = self.windows[j + 1];
            }
            
            self.windows[self.window_count - 1] = win;
            
            // Update z-order to bring to front
            unsafe {
                if let Some(w) = win {
                    (*w).z_order = self.next_z_order;
                    self.next_z_order += 1;
                    ds_set_surface_z_order((*w).surface, (*w).z_order);
                    (*w).invalidated = true;
                    ds_mark_dirty((*w).x, (*w).y, (*w).width, (*w).height);
                }
            }
        }
    }

    fn create_window(&mut self, title: *const c_char, x: i32, y: i32, width: u32, height: u32, flags: u32) -> *mut Window {
        if self.window_count >= 32 {
            return ptr::null_mut();
        }

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
                None => return ptr::null_mut(),
            };
            
            // Create display server surface
            let z_order = self.next_z_order;
            self.next_z_order += 1;
            let surface = ds_create_surface(x, y, width, height, z_order);
            if surface.is_null() {
                return ptr::null_mut();
            }
            
            // Get surface buffer
            let buffer = ds_get_surface_buffer(surface);
            if buffer.is_null() {
                ds_destroy_surface(surface);
                return ptr::null_mut();
            }
            
            // Initialize window in the slot
            let mut new_window = Window {
                id: slot as u32,
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
                buffer: buffer,
                surface: surface,
                z_order: z_order,
                minimized: false,
                maximized: false,
                orig_x: x,
                orig_y: y,
                orig_width: width,
                orig_height: height,
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
            
            WINDOW_POOL[slot] = Some(new_window);
            let window_ptr = WINDOW_POOL[slot].as_mut().unwrap() as *mut Window;
            
            window_ptr
        };

        self.windows[self.window_count] = Some(window);
        self.window_count += 1;
        self.focused_window = Some(window);
        unsafe { 
            (*window).focused = true;
            (*window).invalidated = true;
        }
        self.bring_to_front(window);

        window
    }

    fn destroy_window(&mut self, window: *mut Window) {
        unsafe {
            // Mark window area as dirty
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
            
            // Destroy display server surface
            ds_destroy_surface((*window).surface);
        }
        
        // Find and free the window slot
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
                    }
                    self.windows[self.window_count - 1] = None;
                    self.window_count -= 1;
                    
                    if self.focused_window == Some(window) {
                        self.focused_window = None;
                    }
                    if self.dragging_window == Some(window) {
                        self.dragging_window = None;
                    }
                    break;
                }
            }
        }
    }

    fn invalidate_window(&mut self, window: *mut Window) {
        unsafe {
            (*window).invalidated = true;
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
        }
    }

    fn minimize_window(&mut self, window: *mut Window) {
        unsafe {
            if (*window).minimized {
                return; // Already minimized
            }
            
            (*window).minimized = true;
            (*window).z_order = self.min_z_order;
            self.min_z_order -= 1;
            ds_set_surface_z_order((*window).surface, (*window).z_order);
            
            // Mark old position as dirty
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
            
            // If this was the focused window, clear focus
            if self.focused_window == Some(window) {
                (*window).focused = false;
                self.focused_window = None;
            }
        }
    }

    fn restore_window(&mut self, window: *mut Window) {
        unsafe {
            if !(*window).minimized {
                return; // Not minimized
            }
            
            (*window).minimized = false;
            (*window).z_order = self.next_z_order;
            self.next_z_order += 1;
            ds_set_surface_z_order((*window).surface, (*window).z_order);
            
            // Mark position as dirty
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
            
            // Focus and bring to front
            if let Some(old_focused) = self.focused_window {
                (*old_focused).focused = false;
                (*old_focused).invalidated = true;
                ds_mark_dirty((*old_focused).x, (*old_focused).y, 
                              (*old_focused).width, (*old_focused).height);
            }
            self.focused_window = Some(window);
            (*window).focused = true;
            (*window).invalidated = true;
            self.bring_to_front(window);
        }
    }

    fn maximize_window(&mut self, window: *mut Window) {
        unsafe {
            log_debug(b"maximize_window: called\0");
            
            if (*window).maximized {
                log_info(b"maximize_window: already maximized\0");
                return; // Already maximized
            }
            
            // Save original dimensions
            (*window).orig_x = (*window).x;
            (*window).orig_y = (*window).y;
            (*window).orig_width = (*window).width;
            (*window).orig_height = (*window).height;
            
            // Get framebuffer dimensions
            let fb = self.get_framebuffer();
            let fb_width = (*fb).width as u32;
            let fb_height = (*fb).height as u32;
            
            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char, 
                b"fb_size=%ux%u, orig=%ux%u\0".as_ptr() as *const c_char,
                fb_width, fb_height, (*window).width, (*window).height);
            
            // Check buffer size limit (800x600 = 480000 pixels max)
            const MAX_BUFFER_SIZE: usize = 800 * 600;
            const MAX_WIDTH: u32 = 800;
            const MAX_HEIGHT: u32 = 600;
            
            // Calculate maximize size, limiting to buffer size
            let mut new_width = fb_width;
            let mut new_height = fb_height;
            
            // If framebuffer is larger than buffer limit, scale down proportionally using integer math
            if (new_width * new_height) as usize > MAX_BUFFER_SIZE {
                // Calculate aspect ratio using integer math (multiply by 1000 for precision)
                let aspect_num = fb_width as u64 * 1000;
                let aspect_den = fb_height as u64;
                
                // Try to fit within buffer size while maintaining aspect
                if fb_width > fb_height {
                    // Landscape - limit by width first
                    new_width = MAX_WIDTH;
                    new_height = ((MAX_WIDTH as u64 * aspect_den) / aspect_num) as u32;
                    if (new_width * new_height) as usize > MAX_BUFFER_SIZE {
                        // Still too large, limit by height
                        new_height = MAX_HEIGHT;
                        new_width = ((MAX_HEIGHT as u64 * aspect_num) / aspect_den) as u32;
                        // Final safety check
                        if (new_width * new_height) as usize > MAX_BUFFER_SIZE {
                            // Fallback to a safe size
                            new_width = MAX_WIDTH.min(fb_width);
                            new_height = MAX_HEIGHT.min(fb_height);
                        }
                    }
                } else {
                    // Portrait or square - limit by height first
                    new_height = MAX_HEIGHT;
                    new_width = ((MAX_HEIGHT as u64 * aspect_num) / aspect_den) as u32;
                    if (new_width * new_height) as usize > MAX_BUFFER_SIZE {
                        // Still too large, limit by width
                        new_width = MAX_WIDTH;
                        new_height = ((MAX_WIDTH as u64 * aspect_den) / aspect_num) as u32;
                        // Final safety check
                        if (new_width * new_height) as usize > MAX_BUFFER_SIZE {
                            // Fallback to a safe size
                            new_width = MAX_WIDTH.min(fb_width);
                            new_height = MAX_HEIGHT.min(fb_height);
                        }
                    }
                }
            }
            
            // Final safety check - ensure we never exceed buffer size
            if (new_width * new_height) as usize > MAX_BUFFER_SIZE {
                // Use maximum safe dimensions
                new_width = MAX_WIDTH;
                new_height = MAX_HEIGHT;
            }
            
            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                b"new_size=%ux%u, buffer_size=%u\0".as_ptr() as *const c_char,
                new_width, new_height, (new_width * new_height) as u32);
            
            // Mark old position as dirty
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
            
            // Maximize (centered if limited by buffer size)
            if new_width == fb_width && new_height == fb_height {
                (*window).x = 0;
                (*window).y = 0;
            } else {
                // Center the window if we had to limit the size
                (*window).x = ((fb_width as i32 - new_width as i32) / 2).max(0);
                (*window).y = ((fb_height as i32 - new_height as i32) / 2).max(0);
            }
            
            // Double-check buffer size before updating
            let buffer_size = (new_width * new_height) as usize;
            if buffer_size > MAX_BUFFER_SIZE {
                // Shouldn't happen due to our check above, but be safe
                return;
            }
            
            // Ensure window is not minimized
            if (*window).minimized {
                self.restore_window(window);
            }
            
            // Update window dimensions first
            (*window).width = new_width;
            (*window).height = new_height;
            (*window).maximized = true;
            (*window).invalidated = true;  // Mark as invalidated immediately when dimensions change
            
            // Update surface position first (before size change)
            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                b"setting position to %d,%d\0".as_ptr() as *const c_char,
                (*window).x, (*window).y);
            ds_set_surface_position((*window).surface, (*window).x, (*window).y);
            
            // Then update surface size
            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                b"setting size to %ux%u\0".as_ptr() as *const c_char,
                (*window).width, (*window).height);
            ds_set_surface_size((*window).surface, (*window).width, (*window).height);
            
            // Update buffer pointer (shouldn't change, but be safe)
            (*window).buffer = ds_get_surface_buffer((*window).surface);
            
            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                b"buffer=%p, surface=%p\0".as_ptr() as *const c_char,
                (*window).buffer, (*window).surface);
            
            // Verify surface dimensions match window dimensions
            if !(*window).surface.is_null() {
                let surf_width = (*(*window).surface).width;
                let surf_height = (*(*window).surface).height;
                let surf_x = (*(*window).surface).x;
                let surf_y = (*(*window).surface).y;
                logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                    b"surface: pos=%d,%d size=%ux%u, window: pos=%d,%d size=%ux%u\0".as_ptr() as *const c_char,
                    surf_x, surf_y, surf_width, surf_height, (*window).x, (*window).y, (*window).width, (*window).height);
                
                if surf_width != (*window).width || surf_height != (*window).height {
                    log_error(b"ERROR - surface size mismatch! Restoring original size.\0");
                    // Restore original dimensions
                    (*window).x = (*window).orig_x;
                    (*window).y = (*window).orig_y;
                    (*window).width = (*window).orig_width;
                    (*window).height = (*window).orig_height;
                    (*window).maximized = false;
                    ds_set_surface_size((*window).surface, (*window).width, (*window).height);
                    ds_set_surface_position((*window).surface, (*window).x, (*window).y);
                    (*window).buffer = ds_get_surface_buffer((*window).surface);
                    (*window).invalidated = true;
                    return;
                }
                
                if surf_x != (*window).x || surf_y != (*window).y {
                    log_error(b"ERROR - surface position mismatch!\0");
                }
            }
            
            // Verify buffer is still valid
            if (*window).buffer.is_null() {
                log_error(b"ERROR - buffer is null after resize!\0");
                // Resize failed, restore original dimensions
                (*window).x = (*window).orig_x;
                (*window).y = (*window).orig_y;
                (*window).width = (*window).orig_width;
                (*window).height = (*window).orig_height;
                (*window).maximized = false;
                ds_set_surface_size((*window).surface, (*window).width, (*window).height);
                ds_set_surface_position((*window).surface, (*window).x, (*window).y);
                (*window).buffer = ds_get_surface_buffer((*window).surface);
                return;
            }
            
            // Bring window to front and ensure it's focused
            self.bring_to_front(window);
            if let Some(old_focused) = self.focused_window {
                if old_focused != window {
                    unsafe {
                        (*old_focused).focused = false;
                        (*old_focused).invalidated = true;
                        ds_mark_dirty((*old_focused).x, (*old_focused).y, 
                                      (*old_focused).width, (*old_focused).height);
                    }
                }
            }
            self.focused_window = Some(window);
            unsafe {
                (*window).focused = true;
                (*window).invalidated = true;
                (*window).minimized = false;  // Ensure not minimized
                
                // Log window state before update
                logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                    b"maximize_window: before update - invalidated=%u, pos=%d,%d, size=%ux%u, buffer=%p\0".as_ptr() as *const c_char,
                    if (*window).invalidated { 1 } else { 0 }, (*window).x, (*window).y, (*window).width, (*window).height, (*window).buffer);
                
                ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
            }
            
            // Force immediate render to clear artifacts
            log_debug(b"maximize_window: completed successfully, invalidated=true\0");
            
            // Don't call self.update() here - it would clear the invalidated flag
            // The main loop will call update() and render the window
            // Just ensure the display server knows about the changes
            unsafe {
                ds_render();
            }
            
            // Double-check that invalidated is still true
            unsafe {
                if !(*window).invalidated {
                    log_error(b"maximize_window: ERROR - invalidated flag was cleared!\0");
                    (*window).invalidated = true;
                }
            }
        }
    }

    fn unmaximize_window(&mut self, window: *mut Window) {
        unsafe {
            if !(*window).maximized {
                return; // Not maximized
            }
            
            // Mark old position as dirty
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
            
            // Restore original dimensions
            (*window).x = (*window).orig_x;
            (*window).y = (*window).orig_y;
            (*window).width = (*window).orig_width;
            (*window).height = (*window).orig_height;
            (*window).maximized = false;
            
            // Update surface size and position
            ds_set_surface_size((*window).surface, (*window).width, (*window).height);
            ds_set_surface_position((*window).surface, (*window).x, (*window).y);
            
            // Update buffer pointer
            (*window).buffer = ds_get_surface_buffer((*window).surface);
            
            (*window).invalidated = true;
            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
        }
    }

    fn get_resize_edge(&self, window: *mut Window, mouse_x: i32, mouse_y: i32) -> ResizeEdge {
        unsafe {
            if ((*window).flags & WINDOW_RESIZABLE) == 0 {
                return ResizeEdge::None;
            }
            
            if (*window).maximized {
                return ResizeEdge::None; // Can't resize maximized windows
            }
            
            let wx = (*window).x;
            let wy = (*window).y;
            let ww = (*window).width as i32;
            let wh = (*window).height as i32;
            
            const RESIZE_BORDER: i32 = 8;
            
            let rel_x = mouse_x - wx;
            let rel_y = mouse_y - wy;
            
            let on_left = rel_x < RESIZE_BORDER;
            let on_right = rel_x >= ww - RESIZE_BORDER;
            let on_top = rel_y < RESIZE_BORDER;
            let on_bottom = rel_y >= wh - RESIZE_BORDER;
            
            // Don't resize if in title bar (top 20 pixels)
            if rel_y < 20 {
                return ResizeEdge::None;
            }
            
            match (on_top, on_bottom, on_left, on_right) {
                (true, false, true, false) => ResizeEdge::TopLeft,
                (true, false, false, true) => ResizeEdge::TopRight,
                (false, true, true, false) => ResizeEdge::BottomLeft,
                (false, true, false, true) => ResizeEdge::BottomRight,
                (true, false, false, false) => ResizeEdge::Top,
                (false, true, false, false) => ResizeEdge::Bottom,
                (false, false, true, false) => ResizeEdge::Left,
                (false, false, false, true) => ResizeEdge::Right,
                _ => ResizeEdge::None,
            }
        }
    }

    fn clear_window(&mut self, window: *mut Window, color: u32) {
        unsafe {
            if (*window).buffer.is_null() {
                return;
            }
            
            let buffer = (*window).buffer;
            // Safety: Limit to MAX_BUFFER_SIZE to prevent overflow
            const MAX_BUFFER_SIZE: usize = 800 * 600;
            let requested_size = ((*window).width * (*window).height) as usize;
            let size = if requested_size > MAX_BUFFER_SIZE {
                MAX_BUFFER_SIZE
            } else {
                requested_size
            };
            for i in 0..size {
                *buffer.add(i) = color;
            }
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
            const MAX_TEXT_LENGTH: usize = 1024;
            
            while i < MAX_TEXT_LENGTH && *text_bytes.add(i) != 0 {
                let ch = *text_bytes.add(i) as usize;
                if ch >= 32 && ch <= 126 {
                    draw_char_to_window(window, ch as u8, current_x, y, color);
                    current_x += 8; // 8 pixels per character
                } else if ch == b'\n' as usize {
                    current_x = x;
                }
                i += 1;
            }
        }
    }

    fn handle_mouse(&mut self, mouse_x: i32, mouse_y: i32, left_button: bool) {
        // Track button state for press detection
        let button_just_pressed = left_button && !self.last_mouse_button;
        self.last_mouse_button = left_button;
        
        // Check if we're resizing - continue resizing while button is held
        if let Some(resizing) = self.resizing_window {
            if left_button {
                unsafe {
                    let fb = self.get_framebuffer();
                    let fb_width = (*fb).width as i32;
                    let fb_height = (*fb).height as i32;
                    
                    let mut new_x = (*resizing).x;
                    let mut new_y = (*resizing).y;
                    let mut new_width = self.resize_start_width;
                    let mut new_height = self.resize_start_height;
                    
                    let delta_x = mouse_x - self.resize_start_x;
                    let delta_y = mouse_y - self.resize_start_y;
                    
                    const MIN_WIDTH: u32 = 100;
                    const MIN_HEIGHT: u32 = 100;
                    
                    match self.resize_edge {
                        ResizeEdge::Right => {
                            new_width = (self.resize_start_width as i32 + delta_x) as u32;
                            if new_width < MIN_WIDTH { new_width = MIN_WIDTH; }
                            if new_x + new_width as i32 > fb_width {
                                new_width = (fb_width - new_x) as u32;
                            }
                        },
                        ResizeEdge::Left => {
                            let new_left = self.resize_start_x + delta_x;
                            if new_left < 0 {
                                new_width = (self.resize_start_width as i32 - new_left) as u32;
                                new_x = 0;
                            } else {
                                new_width = (self.resize_start_width as i32 - delta_x) as u32;
                                new_x = new_left;
                            }
                            if new_width < MIN_WIDTH {
                                new_width = MIN_WIDTH;
                                new_x = (*resizing).x + (*resizing).width as i32 - MIN_WIDTH as i32;
                            }
                        },
                        ResizeEdge::Bottom => {
                            new_height = (self.resize_start_height as i32 + delta_y) as u32;
                            if new_height < MIN_HEIGHT { new_height = MIN_HEIGHT; }
                            if new_y + new_height as i32 > fb_height {
                                new_height = (fb_height - new_y) as u32;
                            }
                        },
                        ResizeEdge::Top => {
                            let new_top = self.resize_start_y + delta_y;
                            if new_top < 0 {
                                new_height = (self.resize_start_height as i32 - new_top) as u32;
                                new_y = 0;
                            } else {
                                new_height = (self.resize_start_height as i32 - delta_y) as u32;
                                new_y = new_top;
                            }
                            if new_height < MIN_HEIGHT {
                                new_height = MIN_HEIGHT;
                                new_y = (*resizing).y + (*resizing).height as i32 - MIN_HEIGHT as i32;
                            }
                        },
                        ResizeEdge::BottomRight => {
                            new_width = (self.resize_start_width as i32 + delta_x) as u32;
                            new_height = (self.resize_start_height as i32 + delta_y) as u32;
                            if new_width < MIN_WIDTH { new_width = MIN_WIDTH; }
                            if new_height < MIN_HEIGHT { new_height = MIN_HEIGHT; }
                            if new_x + new_width as i32 > fb_width {
                                new_width = (fb_width - new_x) as u32;
                            }
                            if new_y + new_height as i32 > fb_height {
                                new_height = (fb_height - new_y) as u32;
                            }
                        },
                        ResizeEdge::BottomLeft => {
                            let new_left = self.resize_start_x + delta_x;
                            if new_left < 0 {
                                new_width = (self.resize_start_width as i32 - new_left) as u32;
                                new_x = 0;
                            } else {
                                new_width = (self.resize_start_width as i32 - delta_x) as u32;
                                new_x = new_left;
                            }
                            new_height = (self.resize_start_height as i32 + delta_y) as u32;
                            if new_width < MIN_WIDTH {
                                new_width = MIN_WIDTH;
                                new_x = (*resizing).x + (*resizing).width as i32 - MIN_WIDTH as i32;
                            }
                            if new_height < MIN_HEIGHT { new_height = MIN_HEIGHT; }
                            if new_y + new_height as i32 > fb_height {
                                new_height = (fb_height - new_y) as u32;
                            }
                        },
                        ResizeEdge::TopRight => {
                            new_width = (self.resize_start_width as i32 + delta_x) as u32;
                            let new_top = self.resize_start_y + delta_y;
                            if new_top < 0 {
                                new_height = (self.resize_start_height as i32 - new_top) as u32;
                                new_y = 0;
                            } else {
                                new_height = (self.resize_start_height as i32 - delta_y) as u32;
                                new_y = new_top;
                            }
                            if new_width < MIN_WIDTH { new_width = MIN_WIDTH; }
                            if new_height < MIN_HEIGHT {
                                new_height = MIN_HEIGHT;
                                new_y = (*resizing).y + (*resizing).height as i32 - MIN_HEIGHT as i32;
                            }
                            if new_x + new_width as i32 > fb_width {
                                new_width = (fb_width - new_x) as u32;
                            }
                        },
                        ResizeEdge::TopLeft => {
                            let new_left = self.resize_start_x + delta_x;
                            let new_top = self.resize_start_y + delta_y;
                            if new_left < 0 {
                                new_width = (self.resize_start_width as i32 - new_left) as u32;
                                new_x = 0;
                            } else {
                                new_width = (self.resize_start_width as i32 - delta_x) as u32;
                                new_x = new_left;
                            }
                            if new_top < 0 {
                                new_height = (self.resize_start_height as i32 - new_top) as u32;
                                new_y = 0;
                            } else {
                                new_height = (self.resize_start_height as i32 - delta_y) as u32;
                                new_y = new_top;
                            }
                            if new_width < MIN_WIDTH {
                                new_width = MIN_WIDTH;
                                new_x = (*resizing).x + (*resizing).width as i32 - MIN_WIDTH as i32;
                            }
                            if new_height < MIN_HEIGHT {
                                new_height = MIN_HEIGHT;
                                new_y = (*resizing).y + (*resizing).height as i32 - MIN_HEIGHT as i32;
                            }
                        },
                        _ => {},
                    }
                    
                    // Update window if size or position changed
                    if (*resizing).x != new_x || (*resizing).y != new_y ||
                        (*resizing).width != new_width || (*resizing).height != new_height {
                        // Mark old area as dirty
                        ds_mark_dirty((*resizing).x, (*resizing).y, (*resizing).width, (*resizing).height);
                        
                        (*resizing).x = new_x;
                        (*resizing).y = new_y;
                        (*resizing).width = new_width;
                        (*resizing).height = new_height;
                        
                        // Update surface size and position
                        ds_set_surface_size((*resizing).surface, new_width, new_height);
                        ds_set_surface_position((*resizing).surface, new_x, new_y);
                        
                        // Update buffer pointer
                        (*resizing).buffer = ds_get_surface_buffer((*resizing).surface);
                        
                        (*resizing).invalidated = true;
                        ds_mark_dirty(new_x, new_y, new_width, new_height);
                        
                        // Force immediate render
                        ds_render();
                    }
                }
            } else {
                // Button released - stop resizing
                self.resizing_window = None;
                self.resize_edge = ResizeEdge::None;
                unsafe {
                    ds_update_cursor_position(mouse_x, mouse_y);
                }
            }
            return;
        }
        
        // Check if we're dragging - continue dragging while button is held
        if let Some(dragging) = self.dragging_window {
            if left_button {
                unsafe {
                    let old_x = (*dragging).x;
                    let old_y = (*dragging).y;
                    
                    (*dragging).x = mouse_x - self.drag_offset_x;
                    (*dragging).y = mouse_y - self.drag_offset_y;
                    
                    // Clamp to screen bounds (unless maximized)
                    if !(*dragging).maximized {
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
                    }
                    
                    // Invalidate if position changed
                    if old_x != (*dragging).x || old_y != (*dragging).y {
                        // Ensure window stays at top z-order (only update if needed)
                        if (*dragging).z_order < self.next_z_order - 1 {
                            (*dragging).z_order = self.next_z_order;
                            self.next_z_order += 1;
                            ds_set_surface_z_order((*dragging).surface, (*dragging).z_order);
                        }
                        
                        // Update surface position in display server (marks old and new positions as dirty)
                        ds_set_surface_position((*dragging).surface, (*dragging).x, (*dragging).y);
                        
                        // Force immediate render to clear artifacts and show window at new position
                        ds_render();
                    }
                }
            } else {
                // Button released - stop dragging
                self.dragging_window = None;
                unsafe {
                    ds_update_cursor_position(mouse_x, mouse_y);
                }
            }
            return;
        }
        
        // Not dragging or resizing - update cursor position normally
        unsafe {
            ds_update_cursor_position(mouse_x, mouse_y);
        }

        // Check for window focus and drag start
        if button_just_pressed {
            // Check windows in reverse order (top to bottom), skip minimized windows
            for i in (0..self.window_count).rev() {
                if let Some(window) = self.windows[i] {
                    unsafe {
                        // Skip minimized windows
                        if (*window).minimized {
                            continue;
                        }
                        
                        let wx = (*window).x;
                        let wy = (*window).y;
                        let ww = (*window).width as i32;
                        let wh = (*window).height as i32;
                        
                        if mouse_x >= wx && mouse_x < wx + ww &&
                           mouse_y >= wy && mouse_y < wy + wh {
                            
                            // Check for resize edge first (if not in title bar)
                            if mouse_y >= wy + 20 {
                                let resize_edge = self.get_resize_edge(window, mouse_x, mouse_y);
                                if resize_edge != ResizeEdge::None {
                                    self.resizing_window = Some(window);
                                    self.resize_edge = resize_edge;
                                    self.resize_start_x = mouse_x;
                                    self.resize_start_y = mouse_y;
                                    self.resize_start_width = (*window).width;
                                    self.resize_start_height = (*window).height;
                                    
                                    // Focus window
                                    if let Some(old_focused) = self.focused_window {
                                        if old_focused != window {
                                            (*old_focused).focused = false;
                                            (*old_focused).invalidated = true;
                                            ds_mark_dirty((*old_focused).x, (*old_focused).y, 
                                                          (*old_focused).width, (*old_focused).height);
                                        }
                                    }
                                    self.focused_window = Some(window);
                                    (*window).focused = true;
                                    (*window).invalidated = true;
                                    self.bring_to_front(window);
                                    return;
                                }
                            }
                            
                            // Check if click is on control buttons
                            let mut button_x = wx + ww as i32 - 18;
                            
                            // Close button
                            if ((*window).flags & WINDOW_CLOSABLE) != 0 {
                                let close_x_start = button_x;
                                let close_x_end = close_x_start + 16;
                                let close_y_start = wy + 2;
                                let close_y_end = close_y_start + 16;
                                
                                if mouse_x >= close_x_start && mouse_x < close_x_end &&
                                   mouse_y >= close_y_start && mouse_y < close_y_end {
                                    self.destroy_window(window);
                                    return;
                                }
                                button_x -= 20;
                            }
                            
                            // Maximize button
                            let max_x_start = button_x;
                            let max_x_end = max_x_start + 16;
                            let max_y_start = wy + 2;
                            let max_y_end = max_y_start + 16;
                            
                            if mouse_x >= max_x_start && mouse_x < max_x_end &&
                               mouse_y >= max_y_start && mouse_y < max_y_end {
                                if (*window).maximized {
                                    self.unmaximize_window(window);
                                } else {
                                    self.maximize_window(window);
                                }
                                return;
                            }
                            button_x -= 20;
                            
                            // Minimize button
                            let min_x_start = button_x;
                            let min_x_end = min_x_start + 16;
                            let min_y_start = wy + 2;
                            let min_y_end = min_y_start + 16;
                            
                            if mouse_x >= min_x_start && mouse_x < min_x_end &&
                               mouse_y >= min_y_start && mouse_y < min_y_end {
                                self.minimize_window(window);
                                return;
                            }
                            
                            // Check if click is in title bar (top 20 pixels)
                            let title_bar_y_start = wy;
                            let title_bar_y_end = wy + 20;
                            
                            if mouse_y >= title_bar_y_start && mouse_y < title_bar_y_end {
                                // Focus this window and bring to front
                                if let Some(old_focused) = self.focused_window {
                                    if old_focused != window {
                                        (*old_focused).focused = false;
                                        (*old_focused).invalidated = true;
                                        ds_mark_dirty((*old_focused).x, (*old_focused).y, 
                                                      (*old_focused).width, (*old_focused).height);
                                    }
                                }
                                self.focused_window = Some(window);
                                (*window).focused = true;
                                (*window).invalidated = true;
                                ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
                                self.bring_to_front(window);
                                
                                // Start dragging if window is movable and not maximized
                                if ((*window).flags & WINDOW_MOVABLE) != 0 && !(*window).maximized {
                                    self.dragging_window = Some(window);
                                    self.drag_offset_x = mouse_x - wx;
                                    self.drag_offset_y = mouse_y - wy;
                                }
                            } else {
                                // Click in window content - just focus
                                if let Some(old_focused) = self.focused_window {
                                    if old_focused != window {
                                        (*old_focused).focused = false;
                                        (*old_focused).invalidated = true;
                                        ds_mark_dirty((*old_focused).x, (*old_focused).y, 
                                                      (*old_focused).width, (*old_focused).height);
                                    }
                                }
                                self.focused_window = Some(window);
                                (*window).focused = true;
                                (*window).invalidated = true;
                                ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
                                self.bring_to_front(window);
                            }
                            break; // Stop checking other windows
                        }
                    }
                }
            }
        }
    }

    fn update(&mut self) {
        // Render all invalidated windows (skip minimized windows)
        // First pass: render invalidated windows
        unsafe {
            for i in 0..self.window_count {
                if let Some(window) = self.windows[i] {
                    // Skip minimized windows
                    if (*window).minimized {
                        log_debug(b"update: skipping minimized window\0");
                        continue;
                    }
                    
                    if (*window).invalidated {
                        logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                            b"update: rendering window id=%u, pos=%d,%d, size=%ux%u, buffer=%p\0".as_ptr() as *const c_char,
                            (*window).id, (*window).x, (*window).y, (*window).width, (*window).height, (*window).buffer);
                        
                        if (*window).buffer.is_null() {
                            log_error(b"update: window buffer is null!\0");
                            continue;
                        }
                        // Clear window buffer
                        self.clear_window(window, 0x2d2d2d); // Window background
                        
                        // Draw window border and title bar
                        let title_color = if (*window).focused { 0x4a90e2 } else { 0x404040 };
                        self.draw_filled_rect_to_window(window, 0, 0, (*window).width, 20, title_color);
                        
                        // Draw title text
                        let title_ptr = (*window).title.as_ptr() as *const c_char;
                        self.draw_text_to_window(window, title_ptr, 4, 4, 0xffffff);
                        
                        // Draw window control buttons (minimize, maximize, close)
                        let mut button_x = (*window).width as i32 - 18;
                        
                        // Close button
                        if ((*window).flags & WINDOW_CLOSABLE) != 0 {
                            self.draw_filled_rect_to_window(window, button_x, 2, 16, 16, 0xff4444);
                            draw_char_to_window(window, b'X', button_x + 2, 4, 0xffffff);
                            button_x -= 20;
                        }
                        
                        // Maximize button
                        self.draw_filled_rect_to_window(window, button_x, 2, 16, 16, 0x4444ff);
                        if (*window).maximized {
                            draw_char_to_window(window, b'R', button_x + 2, 4, 0xffffff); // Restore
                        } else {
                            draw_char_to_window(window, b'M', button_x + 2, 4, 0xffffff); // Maximize
                        }
                        button_x -= 20;
                        
                        // Minimize button
                        self.draw_filled_rect_to_window(window, button_x, 2, 16, 16, 0x44ff44);
                        draw_char_to_window(window, b'_', button_x + 2, 4, 0xffffff);
                        
                        // Call custom draw callback if set
                        if let Some(callback) = (*window).draw_callback {
                            callback(window);
                            // Check if callback re-invalidated the window (for continuous animations)
                            // If so, preserve the invalidation so it gets redrawn on next update
                            if (*window).invalidated {
                                // Window was re-invalidated by callback - keep it invalidated
                                // This allows continuous animations to work
                                ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
                                continue; // Skip clearing invalidated flag
                            }
                        }
                        
                        (*window).invalidated = false;
                        ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
                        
                        // Log successful render, especially for maximized windows
                        if (*window).maximized {
                            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                                b"update: maximized window id=%u rendered, pos=%d,%d, size=%ux%u\0".as_ptr() as *const c_char,
                                (*window).id, (*window).x, (*window).y, (*window).width, (*window).height);
                        } else {
                            log_debug(b"update: window rendered successfully\0");
                        }
                    } else {
                        // Window not invalidated - check if it should be visible
                        if (*window).maximized {
                            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                                b"update: maximized window id=%u not invalidated, pos=%d,%d, size=%ux%u, buffer=%p\0".as_ptr() as *const c_char,
                                (*window).id, (*window).x, (*window).y, (*window).width, (*window).height, (*window).buffer);
                            
                            // Maximized windows should always be invalidated to ensure they're rendered
                            // This is a safety check - if a maximized window is not invalidated,
                            // force it to be invalidated and render it immediately
                            log_error(b"update: maximized window not invalidated - forcing invalidation and rendering\0");
                            (*window).invalidated = true;
                            ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
                            
                            // Now render it (same code as above, but inline here)
                            logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                                b"update: rendering maximized window id=%u (forced), pos=%d,%d, size=%ux%u, buffer=%p\0".as_ptr() as *const c_char,
                                (*window).id, (*window).x, (*window).y, (*window).width, (*window).height, (*window).buffer);
                            
                            if !(*window).buffer.is_null() {
                                // Clear window buffer
                                self.clear_window(window, 0x2d2d2d);
                                
                                // Draw window border and title bar
                                let title_color = if (*window).focused { 0x4a90e2 } else { 0x404040 };
                                self.draw_filled_rect_to_window(window, 0, 0, (*window).width, 20, title_color);
                                
                                // Draw title text
                                let title_ptr = (*window).title.as_ptr() as *const c_char;
                                self.draw_text_to_window(window, title_ptr, 4, 4, 0xffffff);
                                
                                // Draw window control buttons
                                let mut button_x = (*window).width as i32 - 18;
                                if ((*window).flags & WINDOW_CLOSABLE) != 0 {
                                    self.draw_filled_rect_to_window(window, button_x, 2, 16, 16, 0xff4444);
                                    draw_char_to_window(window, b'X', button_x + 2, 4, 0xffffff);
                                    button_x -= 20;
                                }
                                self.draw_filled_rect_to_window(window, button_x, 2, 16, 16, 0x4444ff);
                                draw_char_to_window(window, b'R', button_x + 2, 4, 0xffffff);
                                button_x -= 20;
                                self.draw_filled_rect_to_window(window, button_x, 2, 16, 16, 0x44ff44);
                                draw_char_to_window(window, b'_', button_x + 2, 4, 0xffffff);
                                
                                // Call custom draw callback if set
                                if let Some(callback) = (*window).draw_callback {
                                    callback(window);
                                }
                                
                                (*window).invalidated = false;
                                ds_mark_dirty((*window).x, (*window).y, (*window).width, (*window).height);
                                logger_rust_log_fmt(0, b"WM\0".as_ptr() as *const c_char,
                                    b"update: maximized window id=%u rendered (forced), pos=%d,%d, size=%ux%u\0".as_ptr() as *const c_char,
                                    (*window).id, (*window).x, (*window).y, (*window).width, (*window).height);
                            } else {
                                log_error(b"update: maximized window buffer is null - cannot render\0");
                            }
                        }
                    }
                }
            }
        }
        
        // Request display server to render
        unsafe {
            ds_render();
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
        _ => [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00],
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
pub extern "C" fn wm_bring_to_front(window: *mut Window) {
    unsafe {
        if let Some(ref mut wm) = WM_STATE {
            wm.bring_to_front(window);
        }
    }
}
