#![no_std]
#![no_main]

// Panic handler for bare metal
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

use core::ptr;
use core::ffi::{c_char, c_int, c_void};

// External GPU functions
extern "C" {
    fn gpu_is_available() -> bool;
    fn gpu_blit(
        dst: *mut u32,
        dst_pitch: u32,
        src: *const u32,
        src_pitch: u32,
        width: u32,
        height: u32,
    );
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

// Surface structure - represents a drawable surface (window content)
#[repr(C)]
pub struct Surface {
    pub id: u32,
    pub x: i32,
    pub y: i32,
    pub width: u32,
    pub height: u32,
    pub buffer: *mut u32,  // Surface content buffer
    pub z_order: i32,      // Z-order for compositing (higher = on top)
}

// Dirty rectangle for region-based redraw
#[derive(Copy, Clone, Debug)]
struct DirtyRect {
    x: i32,
    y: i32,
    width: u32,
    height: u32,
    valid: bool,
}

impl DirtyRect {
    fn new() -> Self {
        DirtyRect {
            x: 0,
            y: 0,
            width: 0,
            height: 0,
            valid: false,
        }
    }
    
    fn union(&mut self, other: &DirtyRect) {
        if !other.valid {
            return;
        }
        
        if !self.valid {
            *self = *other;
            return;
        }
        
        let left = self.x.min(other.x);
        let top = self.y.min(other.y);
        let right = (self.x + self.width as i32).max(other.x + other.width as i32);
        let bottom = (self.y + self.height as i32).max(other.y + other.height as i32);
        
        self.x = left;
        self.y = top;
        self.width = (right - left) as u32;
        self.height = (bottom - top) as u32;
        self.valid = true;
    }
    
    fn clear(&mut self) {
        self.valid = false;
    }
}

// Display server state
struct DisplayServer {
    framebuffer: *mut LimineFramebuffer,
    surfaces: [Option<*mut Surface>; 32],
    surface_count: usize,
    desktop_color: u32,
    backbuffer_width: u32,
    backbuffer_height: u32,
    backbuffer_initialized: bool,
    dirty_rect: DirtyRect,
    full_redraw: bool,
    desktop_cleared: bool,
    mouse_x: i32,
    mouse_y: i32,
    last_cursor_x: i32,
    last_cursor_y: i32,
    cursor_backup: [u32; (12 + 2) * (16 + 2)],
    cursor_backup_valid: bool,
    wallpaper_width: u32,
    wallpaper_height: u32,
    has_wallpaper: bool,
}

// Backbuffer for double buffering - statically allocated
// Support up to 4K resolution: 3840 * 2160 = 8,294,400 pixels
const MAX_BACKBUFFER_SIZE: usize = 3840 * 2160;
static mut BACKBUFFER: [u32; MAX_BACKBUFFER_SIZE] = [0; MAX_BACKBUFFER_SIZE];

// Wallpaper buffer - store decoded image for desktop background
const MAX_WALLPAPER_SIZE: usize = 1920 * 1080; // Support up to Full HD
static mut WALLPAPER_BUFFER: [u32; MAX_WALLPAPER_SIZE] = [0; MAX_WALLPAPER_SIZE];

// Surface pool for static allocation
static mut SURFACE_POOL: [Option<Surface>; 32] = [const { None }; 32];
const MAX_BUFFER_SIZE: usize = 800 * 600; // VGA resolution
static mut BUFFER_POOL: [[u32; MAX_BUFFER_SIZE]; 32] = [[0; MAX_BUFFER_SIZE]; 32];

// Display server state
static mut DS_STATE: Option<DisplayServer> = None;

impl DisplayServer {
    fn new(framebuffer: *mut LimineFramebuffer) -> Self {
        let mut ds = DisplayServer {
            framebuffer,
            surfaces: [None; 32],
            surface_count: 0,
            desktop_color: 0x0d1117, // Match terminal background (dark gray)
            backbuffer_width: 0,
            backbuffer_height: 0,
            backbuffer_initialized: false,
            dirty_rect: DirtyRect::new(),
            full_redraw: true,
            desktop_cleared: false,
            mouse_x: 0,
            mouse_y: 0,
            last_cursor_x: -1,
            last_cursor_y: -1,
            cursor_backup: [0; (12 + 2) * (16 + 2)],
            cursor_backup_valid: false,
            wallpaper_width: 0,
            wallpaper_height: 0,
            has_wallpaper: false,
        };
        
        // Initialize backbuffer dimensions
        unsafe {
            if !framebuffer.is_null() {
                ds.backbuffer_width = (*framebuffer).width as u32;
                ds.backbuffer_height = (*framebuffer).height as u32;
            }
        }
        
        ds
    }

    fn get_framebuffer(&self) -> *mut LimineFramebuffer {
        self.framebuffer
    }
    
    fn get_backbuffer(&self) -> *mut u32 {
        unsafe {
            BACKBUFFER.as_mut_ptr()
        }
    }
    
    fn mark_dirty(&mut self, x: i32, y: i32, width: u32, height: u32) {
        let rect = DirtyRect {
            x,
            y,
            width,
            height,
            valid: true,
        };
        self.dirty_rect.union(&rect);
    }
    
    fn mark_full_dirty(&mut self) {
        unsafe {
            let fb = self.get_framebuffer();
            if !fb.is_null() {
                self.dirty_rect = DirtyRect {
                    x: 0,
                    y: 0,
                    width: (*fb).width as u32,
                    height: (*fb).height as u32,
                    valid: true,
                };
                self.full_redraw = true;
            }
        }
    }

    fn create_surface(&mut self, x: i32, y: i32, width: u32, height: u32, z_order: i32) -> *mut Surface {
        if self.surface_count >= 32 {
            return ptr::null_mut();
        }

        let surface = unsafe {
            // Find an empty slot
            let mut slot_idx = None;
            for i in 0..32 {
                if SURFACE_POOL[i].is_none() {
                    slot_idx = Some(i);
                    break;
                }
            }
            
            let slot = match slot_idx {
                Some(idx) => idx,
                None => return ptr::null_mut(),
            };
            
            let buffer_size = (width * height) as usize;
            if buffer_size > MAX_BUFFER_SIZE {
                return ptr::null_mut();
            }
            
            let mut new_surface = Surface {
                id: slot as u32,
                x,
                y,
                width,
                height,
                buffer: ptr::null_mut(),
                z_order,
            };
            
            new_surface.buffer = BUFFER_POOL[slot].as_mut_ptr();
            
            SURFACE_POOL[slot] = Some(new_surface);
            SURFACE_POOL[slot].as_mut().unwrap() as *mut Surface
        };

        self.surfaces[self.surface_count] = Some(surface);
        self.surface_count += 1;
        
        // Sort surfaces by z-order
        self.sort_surfaces_by_z_order();
        
        surface
    }

    fn destroy_surface(&mut self, surface: *mut Surface) {
        unsafe {
            // Mark surface area as dirty
            self.mark_dirty((*surface).x, (*surface).y, (*surface).width, (*surface).height);
        }
        
        // Find and remove from array
        for i in 0..self.surface_count {
            if self.surfaces[i] == Some(surface) {
                // Remove from array
                for j in i..self.surface_count - 1 {
                    self.surfaces[j] = self.surfaces[j + 1];
                }
                self.surfaces[self.surface_count - 1] = None;
                self.surface_count -= 1;
                
                // Free the surface slot
                unsafe {
                    let surface_id = (*surface).id as usize;
                    if surface_id < 32 {
                        SURFACE_POOL[surface_id] = None;
                    }
                }
                break;
            }
        }
    }

    fn set_surface_position(&mut self, surface: *mut Surface, x: i32, y: i32) {
        unsafe {
            let old_x = (*surface).x;
            let old_y = (*surface).y;
            let width = (*surface).width;
            let height = (*surface).height;
            
            (*surface).x = x;
            (*surface).y = y;
            
            // Mark old and new positions as dirty
            self.mark_dirty(old_x, old_y, width, height);
            self.mark_dirty(x, y, width, height);
        }
    }

    fn set_surface_z_order(&mut self, surface: *mut Surface, z_order: i32) {
        unsafe {
            (*surface).z_order = z_order;
            self.mark_dirty((*surface).x, (*surface).y, (*surface).width, (*surface).height);
        }
        self.sort_surfaces_by_z_order();
    }

    fn set_surface_size(&mut self, surface: *mut Surface, width: u32, height: u32) {
        unsafe {
            let old_x = (*surface).x;
            let old_y = (*surface).y;
            let old_width = (*surface).width;
            let old_height = (*surface).height;
            
            let buffer_size = (width * height) as usize;
            if buffer_size > MAX_BUFFER_SIZE {
                return; // New size too large
            }
            
            (*surface).width = width;
            (*surface).height = height;
            
            // Mark old and new areas as dirty
            self.mark_dirty(old_x, old_y, old_width, old_height);
            self.mark_dirty(old_x, old_y, width, height);
        }
    }

    fn sort_surfaces_by_z_order(&mut self) {
        // Simple bubble sort by z_order
        for i in 0..self.surface_count {
            for j in 0..self.surface_count - i - 1 {
                unsafe {
                    let z1 = if let Some(s1) = self.surfaces[j] { (*s1).z_order } else { i32::MIN };
                    let z2 = if let Some(s2) = self.surfaces[j + 1] { (*s2).z_order } else { i32::MIN };
                    if z1 > z2 {
                        self.surfaces.swap(j, j + 1);
                    }
                }
            }
        }
    }

    fn get_surface_buffer(&self, surface: *mut Surface) -> *mut u32 {
        unsafe {
            (*surface).buffer
        }
    }

    fn clear_backbuffer(&mut self) {
        unsafe {
            let backbuffer = self.get_backbuffer();
            let size = (self.backbuffer_width * self.backbuffer_height) as usize;
            if size > MAX_BACKBUFFER_SIZE {
                return;
            }
            
            let color = self.desktop_color;
            for i in 0..size {
                *backbuffer.add(i) = color;
            }
        }
    }
    
    fn render_desktop_to_backbuffer(&mut self, dirty: &DirtyRect) {
        unsafe {
            if !dirty.valid {
                return;
            }
            
            let backbuffer = self.get_backbuffer();
            let bb_width = self.backbuffer_width as usize;
            
            if self.has_wallpaper && self.wallpaper_width > 0 && self.wallpaper_height > 0 {
                // Draw wallpaper in dirty region
                let wp_width = self.wallpaper_width as usize;
                let wp_height = self.wallpaper_height as usize;
                
                let start_x = dirty.x.max(0) as usize;
                let start_y = dirty.y.max(0) as usize;
                let end_x = ((dirty.x + dirty.width as i32).min(self.backbuffer_width as i32)).max(0) as usize;
                let end_y = ((dirty.y + dirty.height as i32).min(self.backbuffer_height as i32)).max(0) as usize;
                
                for y in start_y..end_y {
                    for x in start_x..end_x {
                        // Simple nearest-neighbor scaling
                        let src_y = (y * wp_height) / self.backbuffer_height as usize;
                        let src_x = (x * wp_width) / self.backbuffer_width as usize;
                        let src_idx = src_y * wp_width + src_x;
                        
                        if src_idx < MAX_WALLPAPER_SIZE {
                            let pixel = WALLPAPER_BUFFER[src_idx];
                            *backbuffer.add(y * bb_width + x) = pixel;
                        }
                    }
                }
            } else {
                // Fill dirty region with solid color
                let color = self.desktop_color;
                let start_x = dirty.x.max(0) as usize;
                let start_y = dirty.y.max(0) as usize;
                let end_x = ((dirty.x + dirty.width as i32).min(self.backbuffer_width as i32)).max(0) as usize;
                let end_y = ((dirty.y + dirty.height as i32).min(self.backbuffer_height as i32)).max(0) as usize;
                
                for y in start_y..end_y {
                    for x in start_x..end_x {
                        *backbuffer.add(y * bb_width + x) = color;
                    }
                }
            }
        }
    }

    fn surface_overlaps_dirty(&self, surface: *mut Surface, dirty: &DirtyRect) -> bool {
        if !dirty.valid {
            return false;
        }
        
        unsafe {
            let sx = (*surface).x;
            let sy = (*surface).y;
            let sw = (*surface).width as i32;
            let sh = (*surface).height as i32;
            
            !(sx + sw <= dirty.x || sx >= dirty.x + dirty.width as i32 ||
              sy + sh <= dirty.y || sy >= dirty.y + dirty.height as i32)
        }
    }

    fn render_surface_to_backbuffer(&mut self, surface: *mut Surface) {
        unsafe {
            if (*surface).buffer.is_null() {
                return;
            }
            
            let backbuffer = self.get_backbuffer();
            let bb_width = self.backbuffer_width as usize;
            let bb_height = self.backbuffer_height as usize;
            let surf_x = (*surface).x as usize;
            let surf_y = (*surface).y as usize;
            let surf_w = (*surface).width as usize;
            let surf_h = (*surface).height as usize;
            let surf_buffer = (*surface).buffer;
            
            // Blit surface buffer to backbuffer
            for y in 0..surf_h {
                let bb_y = surf_y + y;
                if bb_y >= bb_height {
                    break;
                }
                
                let visible_width = core::cmp::min(surf_w, bb_width.saturating_sub(surf_x));
                if visible_width == 0 {
                    continue;
                }
                
                let src = surf_buffer.add(y * surf_w);
                let dst = backbuffer.add(bb_y * bb_width + surf_x);
                
                core::ptr::copy_nonoverlapping(src, dst, visible_width);
            }
        }
    }

    fn save_cursor_background_from_backbuffer(&mut self, x: i32, y: i32) {
        unsafe {
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            const BACKUP_WIDTH: usize = CURSOR_WIDTH + 2;
            const BACKUP_HEIGHT: usize = CURSOR_HEIGHT + 2;
            
            let backbuffer = self.get_backbuffer();
            let bb_width = self.backbuffer_width as usize;
            let bb_height = self.backbuffer_height as usize;
            
            for row in 0..BACKUP_HEIGHT {
                for col in 0..BACKUP_WIDTH {
                    let px = x + col as i32 - 1;
                    let py = y + row as i32 - 1;
                    
                    if px >= 0 && py >= 0 && 
                       px < bb_width as i32 && py < bb_height as i32 {
                        self.cursor_backup[row * BACKUP_WIDTH + col] = 
                            *backbuffer.add((py as usize) * bb_width + (px as usize));
                    } else {
                        self.cursor_backup[row * BACKUP_WIDTH + col] = self.desktop_color;
                    }
                }
            }
            self.cursor_backup_valid = true;
        }
    }

    fn clear_cursor_from_backbuffer(&mut self, x: i32, y: i32) {
        unsafe {
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            const BACKUP_WIDTH: usize = CURSOR_WIDTH + 2;
            const BACKUP_HEIGHT: usize = CURSOR_HEIGHT + 2;
            
            if !self.cursor_backup_valid {
                let backbuffer = self.get_backbuffer();
                let bb_width = self.backbuffer_width as usize;
                let bb_height = self.backbuffer_height as usize;
                
                for row in -1..=(CURSOR_HEIGHT as i32) {
                    for col in -1..=(CURSOR_WIDTH as i32) {
                        let px = x + col;
                        let py = y + row;
                        
                        if px >= 0 && py >= 0 && 
                           px < bb_width as i32 && py < bb_height as i32 {
                            *backbuffer.add((py as usize) * bb_width + (px as usize)) = self.desktop_color;
                        }
                    }
                }
                return;
            }
            
            let backbuffer = self.get_backbuffer();
            let bb_width = self.backbuffer_width as usize;
            let bb_height = self.backbuffer_height as usize;
            
            for row in 0..BACKUP_HEIGHT {
                for col in 0..BACKUP_WIDTH {
                    let px = x + col as i32 - 1;
                    let py = y + row as i32 - 1;
                    
                    if px >= 0 && py >= 0 && 
                       px < bb_width as i32 && py < bb_height as i32 {
                        *backbuffer.add((py as usize) * bb_width + (px as usize)) = 
                            self.cursor_backup[row * BACKUP_WIDTH + col];
                    }
                }
            }
            
            self.cursor_backup_valid = false;
        }
    }

    fn render_cursor_to_backbuffer(&mut self) {
        unsafe {
            const CURSOR_BITMAP: [u16; 16] = [
                0b110000000000,
                0b111000000000,
                0b111100000000,
                0b111110000000,
                0b111111000000,
                0b111111100000,
                0b111111110000,
                0b111111111000,
                0b111111100000,
                0b111111100000,
                0b110110000000,
                0b110011000000,
                0b100001100000,
                0b000001100000,
                0b000000110000,
                0b000000110000
            ];
            
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            const CURSOR_COLOR: u32 = 0xFFFFFF;
            const CURSOR_OUTLINE_COLOR: u32 = 0x000000;
            
            let x = self.mouse_x;
            let y = self.mouse_y;
            
            // Clear old cursor position if it moved
            if self.last_cursor_x >= 0 && self.last_cursor_y >= 0 && 
               (self.last_cursor_x != x || self.last_cursor_y != y) {
                self.clear_cursor_from_backbuffer(self.last_cursor_x, self.last_cursor_y);
                self.mark_dirty(self.last_cursor_x - 1, self.last_cursor_y - 1, 
                              CURSOR_WIDTH as u32 + 2, CURSOR_HEIGHT as u32 + 2);
            }
            
            // Always render cursor if position is valid
            // Force render every frame to ensure cursor is always visible
            if x >= 0 && y >= 0 && 
               x < self.backbuffer_width as i32 && 
               y < self.backbuffer_height as i32 {
                self.save_cursor_background_from_backbuffer(x, y);
                
                let backbuffer = self.get_backbuffer();
                let bb_width = self.backbuffer_width as usize;
                let bb_height = self.backbuffer_height as usize;
                
                // Draw cursor with black outline first
                for row in 0..CURSOR_HEIGHT {
                    let bitmap_row = CURSOR_BITMAP[row];
                    for col in 0..CURSOR_WIDTH {
                        if (bitmap_row & (1 << (11 - col))) != 0 {
                            for dy in -1..=1 {
                                for dx in -1..=1 {
                                    if dx == 0 && dy == 0 {
                                        continue;
                                    }
                                    let px = x + col as i32 + dx;
                                    let py = y + row as i32 + dy;
                                    
                                    if px >= 0 && py >= 0 && 
                                       px < bb_width as i32 && py < bb_height as i32 {
                                        *backbuffer.add((py as usize) * bb_width + (px as usize)) = CURSOR_OUTLINE_COLOR;
                                    }
                                }
                            }
                        }
                    }
                }
                
                // Draw white cursor pixels
                for row in 0..CURSOR_HEIGHT {
                    let bitmap_row = CURSOR_BITMAP[row];
                    for col in 0..CURSOR_WIDTH {
                        if (bitmap_row & (1 << (11 - col))) != 0 {
                            let px = x + col as i32;
                            let py = y + row as i32;
                            
                            if px >= 0 && py >= 0 && 
                               px < bb_width as i32 && py < bb_height as i32 {
                                *backbuffer.add((py as usize) * bb_width + (px as usize)) = CURSOR_COLOR;
                            }
                        }
                    }
                }
                
                self.mark_dirty(x - 1, y - 1, CURSOR_WIDTH as u32 + 2, CURSOR_HEIGHT as u32 + 2);
                
                self.last_cursor_x = x;
                self.last_cursor_y = y;
            }
        }
    }

    fn copy_backbuffer_to_framebuffer(&self, dirty: &DirtyRect) {
        unsafe {
            let fb = self.get_framebuffer();
            if fb.is_null() {
                return;
            }
            
            if !dirty.valid {
                return;
            }
            
            let fb_ptr = (*fb).address;
            let fb_pitch = (*fb).pitch as usize / 4;
            let fb_width = (*fb).width as usize;
            let fb_height = (*fb).height as usize;
            
            let backbuffer = self.get_backbuffer();
            let bb_width = self.backbuffer_width as usize;
            
            let start_x = dirty.x.max(0) as usize;
            let start_y = dirty.y.max(0) as usize;
            let end_x = ((dirty.x + dirty.width as i32).min(fb_width as i32)).max(0) as usize;
            let end_y = ((dirty.y + dirty.height as i32).min(fb_height as i32)).max(0) as usize;
            
            let width = end_x - start_x;
            let height = end_y - start_y;
            
            if width == 0 || height == 0 {
                return;
            }
            
            // Try GPU-accelerated rendering first
            if gpu_is_available() {
                let src_region = backbuffer.add(start_y * bb_width + start_x);
                let dst_region = fb_ptr.add(start_y * fb_pitch + start_x);
                gpu_blit(
                    dst_region,
                    fb_pitch as u32,
                    src_region,
                    bb_width as u32,
                    width as u32,
                    height as u32,
                );
                return;
            }
            
            // Fallback to CPU-based copy
            for y in start_y..end_y {
                if width > 0 {
                    let src = backbuffer.add(y * bb_width + start_x);
                    let dst = fb_ptr.add(y * fb_pitch + start_x);
                    core::ptr::copy_nonoverlapping(src, dst, width);
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
            
            // Initialize backbuffer on first render
            if !self.backbuffer_initialized {
                self.backbuffer_width = (*fb).width as u32;
                self.backbuffer_height = (*fb).height as u32;
                self.backbuffer_initialized = true;
                self.full_redraw = true;
            }
            
            let needs_full_redraw = self.full_redraw || !self.desktop_cleared;
            
            if needs_full_redraw {
                self.clear_backbuffer();
                self.desktop_cleared = true;
                self.full_redraw = false;
                self.dirty_rect = DirtyRect {
                    x: 0,
                    y: 0,
                    width: self.backbuffer_width,
                    height: self.backbuffer_height,
                    valid: true,
                };
            }
            
            let dirty_rect_copy = self.dirty_rect;
            
            // Render desktop/wallpaper in dirty regions
            if dirty_rect_copy.valid {
                self.render_desktop_to_backbuffer(&dirty_rect_copy);
            }
            
            // Render surfaces in z-order (bottom to top) - only in dirty regions
            if dirty_rect_copy.valid {
                for i in 0..self.surface_count {
                    if let Some(surface) = self.surfaces[i] {
                        if self.surface_overlaps_dirty(surface, &dirty_rect_copy) {
                            self.render_surface_to_backbuffer(surface);
                        }
                    }
                }
            }
            
            // Always render cursor last on backbuffer
            self.render_cursor_to_backbuffer();
            
            // Always include cursor area in dirty rectangle (cursor should always be visible)
            const CURSOR_WIDTH: u32 = 12;
            const CURSOR_HEIGHT: u32 = 16;
            
            // Always include cursor area in dirty rectangle if mouse position is valid
            // This ensures the cursor is always visible, even when nothing else changes
            if self.mouse_x >= 0 && self.mouse_y >= 0 && 
               self.mouse_x < self.backbuffer_width as i32 && 
               self.mouse_y < self.backbuffer_height as i32 {
                let cursor_x = (self.mouse_x - 1).max(0);
                let cursor_y = (self.mouse_y - 1).max(0);
                let cursor_w = CURSOR_WIDTH + 2;
                let cursor_h = CURSOR_HEIGHT + 2;
                
                // Always ensure cursor area is included in dirty rectangle
                if dirty_rect_copy.valid {
                    // Merge cursor area with existing dirty rectangle
                    let min_x = dirty_rect_copy.x.min(cursor_x);
                    let min_y = dirty_rect_copy.y.min(cursor_y);
                    let max_x = (dirty_rect_copy.x + dirty_rect_copy.width as i32).max(cursor_x + cursor_w as i32);
                    let max_y = (dirty_rect_copy.y + dirty_rect_copy.height as i32).max(cursor_y + cursor_h as i32);
                    
                    self.dirty_rect = DirtyRect {
                        x: min_x,
                        y: min_y,
                        width: (max_x - min_x) as u32,
                        height: (max_y - min_y) as u32,
                        valid: true,
                    };
                } else {
                    // If no dirty rect, create one for cursor area to ensure it's always rendered
                    self.dirty_rect = DirtyRect {
                        x: cursor_x,
                        y: cursor_y,
                        width: cursor_w,
                        height: cursor_h,
                        valid: true,
                    };
                }
            } else {
                // Mouse position not valid, use existing dirty rect
                self.dirty_rect = dirty_rect_copy;
            }
            
            // Copy backbuffer to framebuffer (always include cursor area if valid)
            if self.dirty_rect.valid {
                self.copy_backbuffer_to_framebuffer(&self.dirty_rect);
            }
            
            // Clear dirty rectangle after rendering
            self.dirty_rect.clear();
        }
    }

    fn update_cursor_position(&mut self, x: i32, y: i32) {
        let cursor_moved = self.mouse_x != x || self.mouse_y != y;
        
        // Mark old cursor position as dirty before updating
        if cursor_moved && self.last_cursor_x >= 0 && self.last_cursor_y >= 0 {
            const CURSOR_WIDTH: usize = 12;
            const CURSOR_HEIGHT: usize = 16;
            self.mark_dirty(self.last_cursor_x - 1, self.last_cursor_y - 1, 
                          CURSOR_WIDTH as u32 + 2, CURSOR_HEIGHT as u32 + 2);
        }
        
        // Update cursor position
        self.mouse_x = x;
        self.mouse_y = y;
        
        // Always mark new cursor position as dirty to ensure it's rendered
        // This ensures the cursor is visible even on first render
        const CURSOR_WIDTH: usize = 12;
        const CURSOR_HEIGHT: usize = 16;
        self.mark_dirty(x - 1, y - 1, CURSOR_WIDTH as u32 + 2, CURSOR_HEIGHT as u32 + 2);
    }
}

// FFI Functions

#[no_mangle]
pub extern "C" fn ds_init(framebuffer: *mut LimineFramebuffer) {
    unsafe {
        DS_STATE = Some(DisplayServer::new(framebuffer));
    }
}

#[no_mangle]
pub extern "C" fn ds_create_surface(x: c_int, y: c_int, width: u32, height: u32, z_order: c_int) -> *mut Surface {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.create_surface(x, y, width, height, z_order)
        } else {
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_destroy_surface(surface: *mut Surface) {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.destroy_surface(surface);
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_set_surface_position(surface: *mut Surface, x: c_int, y: c_int) {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.set_surface_position(surface, x, y);
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_set_surface_z_order(surface: *mut Surface, z_order: c_int) {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.set_surface_z_order(surface, z_order);
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_set_surface_size(surface: *mut Surface, width: u32, height: u32) {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.set_surface_size(surface, width, height);
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_get_surface_buffer(surface: *mut Surface) -> *mut u32 {
    unsafe {
        if let Some(ref ds) = DS_STATE {
            ds.get_surface_buffer(surface)
        } else {
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_mark_dirty(x: c_int, y: c_int, width: u32, height: u32) {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.mark_dirty(x, y, width, height);
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_update_cursor_position(x: c_int, y: c_int) {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.update_cursor_position(x, y);
        }
    }
}

#[no_mangle]
pub extern "C" fn ds_render() {
    unsafe {
        if let Some(ref mut ds) = DS_STATE {
            ds.render();
        }
    }
}
