#![no_std]
#![no_main]

// Panic handler for bare metal
#[panic_handler]
fn panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

use core::ptr;
use core::ffi::{c_char, c_int};

// File system constants - match C definitions
pub const MAX_FILES: usize = 16;
pub const MAX_FILENAME_LENGTH: usize = 32;
pub const MAX_FILE_SIZE: usize = 1024;
pub const MAX_PATH_LENGTH: usize = 128;

// File types - match C enum
#[repr(C)]
#[derive(Copy, Clone, PartialEq)]
pub enum FileType {
    Regular = 0,
    Directory = 1,
}

// File structure - match C struct
#[repr(C)]
pub struct File {
    pub name: [u8; MAX_FILENAME_LENGTH],
    pub file_type: FileType,
    pub size: usize,
    pub data: [u8; MAX_FILE_SIZE],
    pub used: bool,
    pub created_time: u32,
    pub modified_time: u32,
}

// Directory entry - match C struct
#[repr(C)]
pub struct DirEntry {
    pub name: [u8; MAX_FILENAME_LENGTH],
    pub file_type: FileType,
    pub size: usize,
}

// File system state
static mut FS_STATE: Option<FileSystem> = None;

// File pool for static allocation
static mut FILE_POOL: [File; MAX_FILES] = [const {
    File {
        name: [0; MAX_FILENAME_LENGTH],
        file_type: FileType::Regular,
        size: 0,
        data: [0; MAX_FILE_SIZE],
        used: false,
        created_time: 0,
        modified_time: 0,
    }
}; MAX_FILES];

// Simple time counter (since we don't have real time yet)
static mut CURRENT_TIME: u32 = 0;

struct FileSystem {
    initialized: bool,
}

impl FileSystem {
    fn new() -> Self {
        FileSystem {
            initialized: false,
        }
    }

    fn init(&mut self) {
        if self.initialized {
            return;
        }

        // Clear all files
        unsafe {
            for i in 0..MAX_FILES {
                FILE_POOL[i].used = false;
                FILE_POOL[i].name[0] = 0;
                FILE_POOL[i].file_type = FileType::Regular;
                FILE_POOL[i].size = 0;
                FILE_POOL[i].created_time = 0;
                FILE_POOL[i].modified_time = 0;
            }
        }

        self.initialized = true;

        // Create default files after initialization
        self.create_default_files();
    }

    fn create_default_files(&self) {
        // Create welcome file
        if self.create_file(b"welcome.txt\0".as_ptr() as *const c_char, FileType::Regular) {
            let welcome_text = b"Welcome to DEA OS!\nType 'help' for commands.\n";
            self.write_file(
                b"welcome.txt\0".as_ptr() as *const c_char,
                welcome_text.as_ptr(),
                welcome_text.len(),
            );
        }

        // Create readme file
        if self.create_file(b"readme.txt\0".as_ptr() as *const c_char, FileType::Regular) {
            let readme_text = b"DEA OS File System\n\nCommands:\n- ls\n- cat\n- touch\n- rm\n- write\n- df\n";
            self.write_file(
                b"readme.txt\0".as_ptr() as *const c_char,
                readme_text.as_ptr(),
                readme_text.len(),
            );
        }

        // C Hello World (simplified for testing)
        if self.create_file(b"hello_c.elf\0".as_ptr() as *const c_char, FileType::Regular) {
            let c_message = b"Hello World from C program!\nThis would be a real ELF file in production.\n";
            self.write_file(
                b"hello_c.elf\0".as_ptr() as *const c_char,
                c_message.as_ptr(),
                c_message.len(),
            );
        }

        // Assembly Hello World (simplified for testing)
        if self.create_file(b"hello_asm.elf\0".as_ptr() as *const c_char, FileType::Regular) {
            let asm_message = b"Hello World from Assembly!\nThis would be a real ELF file in production.\n";
            self.write_file(
                b"hello_asm.elf\0".as_ptr() as *const c_char,
                asm_message.as_ptr(),
                asm_message.len(),
            );
        }

        // Demo instruction file
        if self.create_file(b"demo.txt\0".as_ptr() as *const c_char, FileType::Regular) {
            let demo_text = b"DEA OS Program Execution Demo\n============================\n\nTry these commands:\n1. compile hello.elf    - Create test program\n2. exec hello.elf       - Run the program\n3. ps                   - List processes\n4. ls                   - See all files\n\nYour OS can now execute programs!\n";
            self.write_file(
                b"demo.txt\0".as_ptr() as *const c_char,
                demo_text.as_ptr(),
                demo_text.len(),
            );
        }
    }

    fn find_free_slot(&self) -> Option<usize> {
        unsafe {
            for i in 0..MAX_FILES {
                if !FILE_POOL[i].used {
                    return Some(i);
                }
            }
        }
        None
    }

    fn strcmp(s1: *const c_char, s2: *const c_char) -> i32 {
        unsafe {
            let mut i = 0;
            loop {
                let c1 = *s1.add(i);
                let c2 = *s2.add(i);
                if c1 != c2 {
                    return c1 as i32 - c2 as i32;
                }
                if c1 == 0 {
                    return 0;
                }
                i += 1;
            }
        }
    }

    fn strlen(s: *const c_char) -> usize {
        unsafe {
            let mut len = 0;
            while *s.add(len) != 0 {
                len += 1;
            }
            len
        }
    }

    fn strcpy(dest: *mut u8, src: *const c_char) {
        unsafe {
            let mut i = 0;
            loop {
                let c = *src.add(i) as u8;
                *dest.add(i) = c;
                if c == 0 {
                    break;
                }
                i += 1;
                if i >= MAX_FILENAME_LENGTH {
                    break;
                }
            }
        }
    }

    fn find_file(&self, name: *const c_char) -> Option<*mut File> {
        if !self.initialized {
            return None;
        }

        unsafe {
            for i in 0..MAX_FILES {
                if FILE_POOL[i].used {
                    let file_name = FILE_POOL[i].name.as_ptr() as *const c_char;
                    if Self::strcmp(file_name, name) == 0 {
                        return Some(&mut FILE_POOL[i] as *mut File);
                    }
                }
            }
        }
        None
    }

    fn create_file(&self, name: *const c_char, file_type: FileType) -> bool {
        if !self.initialized {
            return false;
        }

        // Check if file already exists
        if self.find_file(name).is_some() {
            return false;
        }

        // Find free slot
        let slot = match self.find_free_slot() {
            Some(s) => s,
            None => return false,
        };

        unsafe {
            FILE_POOL[slot].used = true;
            Self::strcpy(FILE_POOL[slot].name.as_mut_ptr(), name);
            FILE_POOL[slot].file_type = file_type;
            FILE_POOL[slot].size = 0;
            CURRENT_TIME += 1;
            FILE_POOL[slot].created_time = CURRENT_TIME;
            FILE_POOL[slot].modified_time = CURRENT_TIME;
        }

        true
    }

    fn delete_file(&self, name: *const c_char) -> bool {
        if !self.initialized {
            return false;
        }

        unsafe {
            for i in 0..MAX_FILES {
                if FILE_POOL[i].used {
                    let file_name = FILE_POOL[i].name.as_ptr() as *const c_char;
                    if Self::strcmp(file_name, name) == 0 {
                        FILE_POOL[i].used = false;
                        FILE_POOL[i].name[0] = 0;
                        FILE_POOL[i].size = 0;
                        return true;
                    }
                }
            }
        }
        false
    }

    fn write_file(&self, name: *const c_char, data: *const u8, size: usize) -> bool {
        if !self.initialized {
            return false;
        }

        if size > MAX_FILE_SIZE {
            return false;
        }

        let file = match self.find_file(name) {
            Some(f) => f,
            None => {
                // Create file if it doesn't exist
                if !self.create_file(name, FileType::Regular) {
                    return false;
                }
                self.find_file(name).unwrap()
            }
        };

        unsafe {
            // Copy data
            for i in 0..size {
                (*file).data[i] = *data.add(i);
            }
            (*file).size = size;
            CURRENT_TIME += 1;
            (*file).modified_time = CURRENT_TIME;
        }

        true
    }

    fn read_file(&self, name: *const c_char, buffer: *mut u8, size: *mut usize) -> bool {
        if !self.initialized {
            return false;
        }

        let file = match self.find_file(name) {
            Some(f) => f,
            None => return false,
        };

        unsafe {
            // Copy data
            let file_size = (*file).size;
            for i in 0..file_size {
                *buffer.add(i) = (*file).data[i];
            }
            *size = file_size;
        }

        true
    }

    fn list_files(&self, entries: *mut DirEntry, max_entries: usize) -> usize {
        if !self.initialized {
            return 0;
        }

        let mut count = 0;

        unsafe {
            for i in 0..MAX_FILES {
                if count >= max_entries {
                    break;
                }
                if FILE_POOL[i].used {
                    let entry = entries.add(count);
                    Self::strcpy((*entry).name.as_mut_ptr(), FILE_POOL[i].name.as_ptr() as *const c_char);
                    (*entry).file_type = FILE_POOL[i].file_type;
                    (*entry).size = FILE_POOL[i].size;
                    count += 1;
                }
            }
        }

        count
    }

    fn file_exists(&self, name: *const c_char) -> bool {
        self.find_file(name).is_some()
    }

    fn get_free_space(&self) -> usize {
        if !self.initialized {
            return 0;
        }

        let mut free_slots = 0;
        unsafe {
            for i in 0..MAX_FILES {
                if !FILE_POOL[i].used {
                    free_slots += 1;
                }
            }
        }

        free_slots * MAX_FILE_SIZE
    }

    fn get_used_space(&self) -> usize {
        if !self.initialized {
            return 0;
        }

        let mut used = 0;
        unsafe {
            for i in 0..MAX_FILES {
                if FILE_POOL[i].used {
                    used += FILE_POOL[i].size;
                }
            }
        }

        used
    }
}

// FFI Functions - match C interface

#[no_mangle]
pub extern "C" fn fs_init() {
    unsafe {
        if FS_STATE.is_none() {
            FS_STATE = Some(FileSystem::new());
        }
        if let Some(ref mut fs) = FS_STATE {
            fs.init();
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_create_file(name: *const c_char, file_type: c_int) -> bool {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            let ft = if file_type == 0 {
                FileType::Regular
            } else {
                FileType::Directory
            };
            fs.create_file(name, ft)
        } else {
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_delete_file(name: *const c_char) -> bool {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.delete_file(name)
        } else {
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_find_file(name: *const c_char) -> *mut File {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.find_file(name).unwrap_or(ptr::null_mut())
        } else {
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_write_file(name: *const c_char, data: *const u8, size: usize) -> bool {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.write_file(name, data, size)
        } else {
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_read_file(name: *const c_char, buffer: *mut u8, size: *mut usize) -> bool {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.read_file(name, buffer, size)
        } else {
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_list_files(entries: *mut DirEntry, max_entries: c_int) -> c_int {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.list_files(entries, max_entries as usize) as c_int
        } else {
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_file_exists(name: *const c_char) -> bool {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.file_exists(name)
        } else {
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_get_free_space() -> usize {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.get_free_space()
        } else {
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn fs_get_used_space() -> usize {
    unsafe {
        if let Some(ref fs) = FS_STATE {
            fs.get_used_space()
        } else {
            0
        }
    }
}
