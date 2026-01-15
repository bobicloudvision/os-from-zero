#![no_std]
#![no_main]

// Panic handler for bare metal
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

use core::ptr;
use core::ffi::c_void;

// GPU rendering context
#[repr(C)]
pub struct GpuContext {
    framebuffer: *mut u32,
    width: u32,
    height: u32,
    pitch: u32,
    gpu_available: bool,
}

static mut GPU_CONTEXT: Option<GpuContext> = None;

// Initialize GPU rendering context
#[no_mangle]
pub extern "C" fn gpu_init(framebuffer: *mut c_void, width: u32, height: u32, pitch: u32) {
    unsafe {
        GPU_CONTEXT = Some(GpuContext {
            framebuffer: framebuffer as *mut u32,
            width,
            height,
            pitch,
            gpu_available: true,
        });
    }
}

// Check if GPU is available
#[no_mangle]
pub extern "C" fn gpu_is_available() -> bool {
    unsafe {
        GPU_CONTEXT.is_some() && GPU_CONTEXT.as_ref().unwrap().gpu_available
    }
}

// Fast memory copy using optimized operations
// This uses SIMD-like operations when possible
#[no_mangle]
pub extern "C" fn gpu_blit(
    dst: *mut u32,
    dst_pitch: u32,
    src: *const u32,
    src_pitch: u32,
    width: u32,
    height: u32,
) {
    unsafe {
        if dst.is_null() || src.is_null() {
            return;
        }
        
        // Optimized row-by-row copy
        for y in 0..height {
            let dst_row = dst.add((y * dst_pitch) as usize);
            let src_row = src.add((y * src_pitch) as usize);
            
            // Use copy_nonoverlapping for fast memory copy
            core::ptr::copy_nonoverlapping(src_row, dst_row, width as usize);
        }
    }
}

// Fast fill rectangle using optimized operations
#[no_mangle]
pub extern "C" fn gpu_fill_rect(
    buffer: *mut u32,
    pitch: u32,
    x: i32,
    y: i32,
    width: u32,
    height: u32,
    color: u32,
) {
    unsafe {
        if buffer.is_null() {
            return;
        }
        
        // Bounds checking
        if x < 0 || y < 0 {
            return;
        }
        
        let start_x = x as usize;
        let start_y = y as usize;
        let w = width as usize;
        let h = height as usize;
        
        // Fill row by row
        for row in 0..h {
            let row_ptr = buffer.add((start_y + row) * pitch as usize + start_x);
            
            // Fill entire row at once
            for col in 0..w {
                *row_ptr.add(col) = color;
            }
        }
    }
}

// Optimized alpha blending (for future transparency support)
#[no_mangle]
pub extern "C" fn gpu_alpha_blend(
    dst: *mut u32,
    src: *const u32,
    width: u32,
    height: u32,
    alpha: u8,
) {
    unsafe {
        if dst.is_null() || src.is_null() {
            return;
        }
        
        let alpha_f = alpha as u32;
        let inv_alpha = 255 - alpha;
        let inv_alpha_f = inv_alpha as u32;
        
        for y in 0..height {
            for x in 0..width {
                let idx = (y * width + x) as usize;
                let src_pixel = *src.add(idx);
                let dst_pixel = *dst.add(idx);
                
                // Extract color components
                let src_r = ((src_pixel >> 16) & 0xFF) as u32;
                let src_g = ((src_pixel >> 8) & 0xFF) as u32;
                let src_b = (src_pixel & 0xFF) as u32;
                
                let dst_r = ((dst_pixel >> 16) & 0xFF) as u32;
                let dst_g = ((dst_pixel >> 8) & 0xFF) as u32;
                let dst_b = (dst_pixel & 0xFF) as u32;
                
                // Alpha blend
                let r = ((src_r * alpha_f + dst_r * inv_alpha_f) / 255) as u32;
                let g = ((src_g * alpha_f + dst_g * inv_alpha_f) / 255) as u32;
                let b = ((src_b * alpha_f + dst_b * inv_alpha_f) / 255) as u32;
                
                *dst.add(idx) = (r << 16) | (g << 8) | b;
            }
        }
    }
}

// GPU-accelerated rectangle copy (for window moving/resizing)
#[no_mangle]
pub extern "C" fn gpu_copy_rect(
    dst: *mut u32,
    dst_pitch: u32,
    dst_x: i32,
    dst_y: i32,
    src: *const u32,
    src_pitch: u32,
    src_x: i32,
    src_y: i32,
    width: u32,
    height: u32,
) {
    unsafe {
        if dst.is_null() || src.is_null() {
            return;
        }
        
        if dst_x < 0 || dst_y < 0 || src_x < 0 || src_y < 0 {
            return;
        }
        
        let w = width as usize;
        let h = height as usize;
        
        for row in 0..h {
            let src_row = src.add(((src_y as usize + row) * src_pitch as usize) + src_x as usize);
            let dst_row = dst.add(((dst_y as usize + row) * dst_pitch as usize) + dst_x as usize);
            
            core::ptr::copy_nonoverlapping(src_row, dst_row, w);
        }
    }
}

// Clear entire framebuffer (optimized)
#[no_mangle]
pub extern "C" fn gpu_clear(buffer: *mut u32, width: u32, height: u32, color: u32) {
    unsafe {
        if buffer.is_null() {
            return;
        }
        
        let size = (width * height) as usize;
        
        // Fill first row
        for x in 0..width as usize {
            *buffer.add(x) = color;
        }
        
        // Copy first row to all other rows (faster than filling individually)
        let first_row = buffer;
        for y in 1..height as usize {
            let row = buffer.add(y * width as usize);
            core::ptr::copy_nonoverlapping(first_row, row, width as usize);
        }
    }
}

// Get GPU context (for internal use)
fn get_context() -> Option<&'static mut GpuContext> {
    unsafe {
        GPU_CONTEXT.as_mut()
    }
}

// GPU-accelerated rendering to framebuffer
#[no_mangle]
pub extern "C" fn gpu_render_to_framebuffer(
    src: *const u32,
    src_width: u32,
    src_height: u32,
    dst_x: i32,
    dst_y: i32,
) -> bool {
    let ctx = match get_context() {
        Some(c) => c,
        None => return false,
    };
    
    unsafe {
        if ctx.framebuffer.is_null() || src.is_null() {
            return false;
        }
        
        // Bounds checking
        if dst_x < 0 || dst_y < 0 {
            return false;
        }
        
        let dst_w = ctx.width;
        let dst_h = ctx.height;
        let dst_pitch = ctx.pitch;
        
        // Calculate visible region
        let src_start_x = if dst_x < 0 { (-dst_x) as usize } else { 0 };
        let src_start_y = if dst_y < 0 { (-dst_y) as usize } else { 0 };
        let dst_start_x = dst_x.max(0) as usize;
        let dst_start_y = dst_y.max(0) as usize;
        
        let copy_width = core::cmp::min(src_width as usize - src_start_x, dst_w as usize - dst_start_x);
        let copy_height = core::cmp::min(src_height as usize - src_start_y, dst_h as usize - dst_start_y);
        
        if copy_width == 0 || copy_height == 0 {
            return false;
        }
        
        // Copy visible region
        for row in 0..copy_height {
            let src_row = src.add((src_start_y + row) * src_width as usize + src_start_x);
            let dst_row = ctx.framebuffer.add((dst_start_y + row) * dst_pitch as usize + dst_start_x);
            
            core::ptr::copy_nonoverlapping(src_row, dst_row, copy_width);
        }
        
        true
    }
}

// GPU command queue (for future VirtIO GPU support)
#[repr(C)]
#[derive(Copy, Clone)]
pub struct GpuCommand {
    command_type: u32,
    data: [u32; 16],
}

static mut COMMAND_QUEUE: [Option<GpuCommand>; 64] = [const { None }; 64];
static mut COMMAND_QUEUE_HEAD: usize = 0;
static mut COMMAND_QUEUE_TAIL: usize = 0;

#[no_mangle]
pub extern "C" fn gpu_submit_command(cmd: *const GpuCommand) -> bool {
    unsafe {
        if cmd.is_null() {
            return false;
        }
        
        let next_tail = (COMMAND_QUEUE_TAIL + 1) % 64;
        if next_tail == COMMAND_QUEUE_HEAD {
            return false; // Queue full
        }
        
        // Safely copy from raw pointer
        let cmd_copy = core::ptr::read(cmd);
        COMMAND_QUEUE[COMMAND_QUEUE_TAIL] = Some(cmd_copy);
        COMMAND_QUEUE_TAIL = next_tail;
        
        true
    }
}

#[no_mangle]
pub extern "C" fn gpu_process_commands() {
    unsafe {
        while COMMAND_QUEUE_HEAD != COMMAND_QUEUE_TAIL {
            if COMMAND_QUEUE[COMMAND_QUEUE_HEAD].is_some() {
                // Process command (placeholder for future GPU command processing)
                // For now, commands are queued but not processed
                // Access command by reference to avoid move
                let _cmd = &COMMAND_QUEUE[COMMAND_QUEUE_HEAD];
                COMMAND_QUEUE[COMMAND_QUEUE_HEAD] = None;
                COMMAND_QUEUE_HEAD = (COMMAND_QUEUE_HEAD + 1) % 64;
            } else {
                // Skip None entries
                COMMAND_QUEUE_HEAD = (COMMAND_QUEUE_HEAD + 1) % 64;
            }
        }
    }
}
