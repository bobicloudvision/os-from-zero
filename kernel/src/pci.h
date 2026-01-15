#ifndef PCI_H
#define PCI_H

#include <stdint.h>
#include <stdbool.h>

// PCI Configuration Space Registers
#define PCI_CONFIG_VENDOR_ID    0x00
#define PCI_CONFIG_DEVICE_ID    0x02
#define PCI_CONFIG_COMMAND      0x04
#define PCI_CONFIG_STATUS       0x06
#define PCI_CONFIG_CLASS_CODE   0x0B
#define PCI_CONFIG_SUBCLASS     0x0A
#define PCI_CONFIG_HEADER_TYPE  0x0E
#define PCI_CONFIG_BAR0         0x10

// PCI Class Codes
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_DISPLAY_VGA   0x0300
#define PCI_CLASS_DISPLAY_OTHER 0x0380

// PCI Device structure
struct pci_device {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint32_t bar0;  // Base Address Register 0
    bool is_vga;
};

// Function prototypes
uint32_t pci_read_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset);
void pci_write_config(uint8_t bus, uint8_t device, uint8_t function, uint8_t offset, uint32_t value);
bool pci_device_exists(uint8_t bus, uint8_t device, uint8_t function);
void pci_enumerate(void);
struct pci_device* pci_find_device(uint16_t vendor_id, uint16_t device_id);
struct pci_device* pci_find_class(uint8_t class_code, uint8_t subclass);
int pci_get_device_count(void);
struct pci_device* pci_get_device(int index);

#endif // PCI_H
