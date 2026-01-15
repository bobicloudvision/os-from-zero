#include "pci.h"
#include <stddef.h>

// PCI Configuration Space Access Ports
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// Maximum number of PCI devices to track
#define MAX_PCI_DEVICES 32

// Static array to store discovered PCI devices
static struct pci_device pci_devices[MAX_PCI_DEVICES];
static int pci_device_count = 0;

// Read 32-bit value from PCI configuration space
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset) {
    // Create configuration address
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | 0x80000000);
    
    // Write address to configuration address port
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
    
    // Read data from configuration data port
    uint32_t data;
    __asm__ volatile ("inl %1, %0" : "=a"(data) : "Nd"(PCI_CONFIG_DATA));
    
    return data;
}

// Write 32-bit value to PCI configuration space
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value) {
    // Create configuration address
    uint32_t address = (uint32_t)((bus << 16) | (device << 11) | (function << 8) | (offset & 0xFC) | 0x80000000);
    
    // Write address to configuration address port
    __asm__ volatile ("outl %0, %1" : : "a"(address), "Nd"(PCI_CONFIG_ADDRESS));
    
    // Write data to configuration data port
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(PCI_CONFIG_DATA));
}

// Check if a PCI device exists
bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function) {
    uint32_t vendor_id = pci_read_config(bus, device, function, PCI_CONFIG_VENDOR_ID);
    return (vendor_id & 0xFFFF) != 0xFFFF;
}

// Enumerate all PCI devices
void pci_enumerate(void) {
    pci_device_count = 0;
    
    // Scan all buses (0-255), devices (0-31), and functions (0-7)
    for (int bus = 0; bus < 256; bus++) {
        for (uint8_t device = 0; device < 32; device++) {
            // Check if device exists
            if (!pci_device_exists(bus, device, 0)) {
                continue;
            }
            
            // Read device information
            uint32_t vendor_device = pci_read_config(bus, device, 0, PCI_CONFIG_VENDOR_ID);
            uint16_t vendor_id = vendor_device & 0xFFFF;
            uint16_t device_id = (vendor_device >> 16) & 0xFFFF;
            
            uint32_t class_reg = pci_read_config(bus, device, 0, PCI_CONFIG_CLASS_CODE);
            uint8_t class_code = (class_reg >> 16) & 0xFF;
            uint8_t subclass = (class_reg >> 24) & 0xFF;
            
            uint8_t header_type = (pci_read_config(bus, device, 0, PCI_CONFIG_HEADER_TYPE) >> 16) & 0xFF;
            
            // Store device if we have space
            if (pci_device_count < MAX_PCI_DEVICES) {
                struct pci_device *dev = &pci_devices[pci_device_count];
                dev->bus = bus;
                dev->device = device;
                dev->function = 0;
                dev->vendor_id = vendor_id;
                dev->device_id = device_id;
                dev->class_code = class_code;
                dev->subclass = subclass;
                dev->bar0 = pci_read_config(bus, device, 0, PCI_CONFIG_BAR0);
                dev->is_vga = (class_code == PCI_CLASS_DISPLAY && subclass == 0x00);
                
                pci_device_count++;
            }
            
            // Check for multi-function devices
            if ((header_type & 0x80) != 0) {
                // Multi-function device - check other functions
                for (uint8_t function = 1; function < 8; function++) {
                    if (pci_device_exists(bus, device, function)) {
                        vendor_device = pci_read_config(bus, device, function, PCI_CONFIG_VENDOR_ID);
                        vendor_id = vendor_device & 0xFFFF;
                        device_id = (vendor_device >> 16) & 0xFFFF;
                        
                        class_reg = pci_read_config(bus, device, function, PCI_CONFIG_CLASS_CODE);
                        class_code = (class_reg >> 16) & 0xFF;
                        subclass = (class_reg >> 24) & 0xFF;
                        
                        if (pci_device_count < MAX_PCI_DEVICES) {
                            struct pci_device *dev = &pci_devices[pci_device_count];
                            dev->bus = bus;
                            dev->device = device;
                            dev->function = function;
                            dev->vendor_id = vendor_id;
                            dev->device_id = device_id;
                            dev->class_code = class_code;
                            dev->subclass = subclass;
                            dev->bar0 = pci_read_config(bus, device, function, PCI_CONFIG_BAR0);
                            dev->is_vga = (class_code == PCI_CLASS_DISPLAY && subclass == 0x00);
                            
                            pci_device_count++;
                        }
                    }
                }
            }
        }
    }
}

// Find device by vendor and device ID
struct pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].vendor_id == vendor_id && pci_devices[i].device_id == device_id) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Find device by class code
struct pci_device* pci_find_class(uint8_t class_code, uint8_t subclass) {
    for (int i = 0; i < pci_device_count; i++) {
        if (pci_devices[i].class_code == class_code && pci_devices[i].subclass == subclass) {
            return &pci_devices[i];
        }
    }
    return NULL;
}

// Get number of discovered devices
int pci_get_device_count(void) {
    return pci_device_count;
}

// Get device by index
struct pci_device* pci_get_device(int index) {
    if (index >= 0 && index < pci_device_count) {
        return &pci_devices[index];
    }
    return NULL;
}
