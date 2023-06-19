#ifndef _USB_HID_H
#define _USB_HID_H

#include <stdint.h>
#include <pthread.h>
#include "pad.h"

#define LIBUSB_TRANSFER_TYPE_MASK 0x03
#define LIBUSB_ENDPOINT_DIR_MASK 0x80

enum libusb_endpoint_direction {
    /** Out: host-to-device */
    LIBUSB_ENDPOINT_OUT = 0x00,

    /** In: device-to-host */
    LIBUSB_ENDPOINT_IN = 0x80
};

enum libusb_transfer_type {
    /** Control transfer */
    LIBUSB_TRANSFER_TYPE_CONTROL = 0U,

    /** Isochronous transfer */
    LIBUSB_TRANSFER_TYPE_ISOCHRONOUS = 1U,

    /** Bulk transfer */
    LIBUSB_TRANSFER_TYPE_BULK = 2U,

    /** Interrupt transfer */
    LIBUSB_TRANSFER_TYPE_INTERRUPT = 3U,

    /** Bulk stream transfer */
    LIBUSB_TRANSFER_TYPE_BULK_STREAM = 4U
};

// Transparent structs used internally by the Usbd library.
typedef struct libusb_device libusb_device;
typedef struct libusb_device_handle libusb_device_handle;

struct libusb_endpoint_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
    uint8_t bRefresh;
    uint8_t bSynchAddress;
    uint8_t* extra;
    int extraLength;
};

struct libusb_device_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
};

struct libusb_interface_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
    struct libusb_endpoint_descriptor* endpoint;
    const unsigned char* extra;
    int extra_length;
};

struct libusb_interface {
    struct libusb_interface_descriptor* altsetting;
    int num_altsetting;
};

struct libusb_config_descriptor {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t MaxPower;
    struct libusb_interface* interface;
    unsigned char* extra;
    int extra_length;
};

enum libusb_transfer_status {
    LIBUSB_TRANSFER_COMPLETED,
    LIBUSB_TRANSFER_ERROR,
    LIBUSB_TRANSFER_TIMED_OUT,
    LIBUSB_TRANSFER_CANCELLED,
    LIBUSB_TRANSFER_STALL,
    LIBUSB_TRANSFER_NO_DEVICE,
    LIBUSB_TRANSFER_OVERFLOW
};

struct libusb_iso_packet_descriptor {
    unsigned int length;
    unsigned int actual_length;
    enum libusb_transfer_status status;
};

typedef struct libusb_transfer libusb_transfer;
typedef void (*libusb_transfer_cb_fn)(struct libusb_transfer* transfer);

struct libusb_transfer {
    libusb_device_handle* dev_handle;
    uint8_t flags;
    unsigned char endpoint;
    unsigned char type;
    unsigned int timeout;
    enum libusb_transfer_status status;
    int length;
    int actual_length;
    libusb_transfer_cb_fn callback;
    void* user_data;
    unsigned char* buffer;
    int num_iso_packets;
    struct libusb_iso_packet_descriptor iso_packet_desc[0];
};

#define XPAD_PKT_LEN 64

/* The Guitar Hero Live (GHL) Xbox One dongles require a poke
 * every 8 seconds.
 */
#define GHL_GUITAR_POKE_INTERVAL 8 /* In seconds */

/*
 * xbox d-pads should map to buttons, as is required for DDR pads
 * but we map them to axes when possible to simplify things
 */
#define MAP_DPAD_TO_BUTTONS (1 << 0)
#define MAP_TRIGGERS_TO_BUTTONS (1 << 1)
#define MAP_STICKS_TO_NULL (1 << 2)
#define MAP_SELECT_BUTTON (1 << 3)
#define MAP_PADDLES (1 << 4)
#define DANCEPAD_MAP_CONFIG (MAP_DPAD_TO_BUTTONS | MAP_TRIGGERS_TO_BUTTONS | MAP_STICKS_TO_NULL)

#define XTYPE_XBOX 0
#define XTYPE_XBOX360 1
#define XTYPE_XBOX360W 2
#define XTYPE_XBOXONE 3
#define XTYPE_UNKNOWN 4

#define XPAD360W_POWEROFF_TIMEOUT 5

#define PKT_XB 0
#define PKT_XBE1 1
#define PKT_XBE2_FW_OLD 2
#define PKT_XBE2_FW_5_EARLY 3
#define PKT_XBE2_FW_5_11 4

#define QUIRK_360_START_PKT_1 (1 << 0)
#define QUIRK_360_START_PKT_2 (1 << 1)
#define QUIRK_360_START_PKT_3 (1 << 2)
#define QUIRK_GHL_XBOXONE (1 << 3)
#define QUIRK_360_START (QUIRK_360_START_PKT_1 | QUIRK_360_START_PKT_2 | QUIRK_360_START_PKT_3)

struct xpad_device {
    u16 idVendor;
    u16 idProduct;
    char* name;
    u8 mapping;
    u8 xtype;
    u8 quirks;
    u8 reserve;
    //    libusb_device_handle* handle;
};

struct input_report {
    uint8_t* data;
    size_t len;
    struct input_report* next;
};

struct hid_device {
    libusb_device_handle* handle;
    /* USB Configuration Number of the device */
    int config_number;
    /* The interface number of the HID */
    int interface;
    int interface_class;
    int interface_subclass;
    int interface_protocol;

    uint16_t report_descriptor_size;

    /* Endpoint information */
    int input_endpoint;
    int output_endpoint;
    int input_ep_max_packet_size;

    /* Indexes of Strings */
    int manufacturer_index;
    int product_index;
    int serial_index;
    // struct hid_device_info* device_info;
    /* Whether blocking reads are used */
    int blocking; /* boolean */

    /* Read thread objects */
    pthread_t thread;
    pthread_mutex_t mutex; /* Protects input_reports */
    pthread_cond_t condition;
    pthread_barrier_t barrier;
    int shutdown_thread;
    int transfer_loop_finished;
    struct libusb_transfer* transfer;
    /* Quirks */
    int skip_output_report_id;
    int no_output_reports_on_intr_ep;
    /* List of received input reports. */
    struct input_report* input_reports;

    int is_driver_detached;
    u16 idVendor;
    u16 idProduct;
    u8 xtype;
    u8 mapping;
    u8 quirks;
};

static struct xpad_device xpad_device_list[] = {
    {0x0079, 0x18d4, "GPD Win 2 X-Box Controller", 0, XTYPE_XBOX360},
    {0x03eb, 0xff01, "Wooting One (Legacy)", 0, XTYPE_XBOX360},
    {0x03eb, 0xff02, "Wooting Two (Legacy)", 0, XTYPE_XBOX360},
    {0x044f, 0x0f00, "Thrustmaster Wheel", 0, XTYPE_XBOX},
    {0x044f, 0x0f03, "Thrustmaster Wheel", 0, XTYPE_XBOX},
    {0x044f, 0x0f07, "Thrustmaster, Inc. Controller", 0, XTYPE_XBOX},
    {0x044f, 0x0f10, "Thrustmaster Modena GT Wheel", 0, XTYPE_XBOX},
    {0x044f, 0xb326, "Thrustmaster Gamepad GP XID", 0, XTYPE_XBOX360},
    {0x045e, 0x0202, "Microsoft X-Box pad v1 (US)", 0, XTYPE_XBOX},
    {0x045e, 0x0285, "Microsoft X-Box pad (Japan)", 0, XTYPE_XBOX},
    {0x045e, 0x0287, "Microsoft Xbox Controller S", 0, XTYPE_XBOX},
    {0x045e, 0x0288, "Microsoft Xbox Controller S v2", 0, XTYPE_XBOX},
    {0x045e, 0x0289, "Microsoft X-Box pad v2 (US)", 0, XTYPE_XBOX},
    {0x045e, 0x028e, "Microsoft X-Box 360 pad", 0, XTYPE_XBOX360},
    {0x045e, 0x028f, "Microsoft X-Box 360 pad v2", 0, XTYPE_XBOX360},
    {0x045e, 0x0291, "Xbox 360 Wireless Receiver (XBOX)", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX360W},
    {0x045e, 0x02d1, "Microsoft X-Box One pad", 0, XTYPE_XBOXONE},
    {0x045e, 0x02dd, "Microsoft X-Box One pad (Firmware 2015)", 0, XTYPE_XBOXONE},
    {0x045e, 0x02e3, "Microsoft X-Box One Elite pad", MAP_PADDLES, XTYPE_XBOXONE},
    {0x045e, 0x0b00, "Microsoft X-Box One Elite 2 pad", MAP_PADDLES, XTYPE_XBOXONE},
    {0x045e, 0x02ea, "Microsoft X-Box One S pad", 0, XTYPE_XBOXONE},
    {0x045e, 0x0719, "Xbox 360 Wireless Receiver", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX360W},
    {0x045e, 0x0b12, "Microsoft Xbox Series S|X Controller", MAP_SELECT_BUTTON, XTYPE_XBOXONE},
    {0x046d, 0xc21d, "Logitech Gamepad F310", 0, XTYPE_XBOX360},
    {0x046d, 0xc21e, "Logitech Gamepad F510", 0, XTYPE_XBOX360},
    {0x046d, 0xc21f, "Logitech Gamepad F710", 0, XTYPE_XBOX360},
    {0x046d, 0xc242, "Logitech Chillstream Controller", 0, XTYPE_XBOX360},
    {0x046d, 0xca84, "Logitech Xbox Cordless Controller", 0, XTYPE_XBOX},
    {0x046d, 0xca88, "Logitech Compact Controller for Xbox", 0, XTYPE_XBOX},
    {0x046d, 0xca8a, "Logitech Precision Vibration Feedback Wheel", 0, XTYPE_XBOX},
    {0x046d, 0xcaa3, "Logitech DriveFx Racing Wheel", 0, XTYPE_XBOX360},
    {0x056e, 0x2004, "Elecom JC-U3613M", 0, XTYPE_XBOX360},
    {0x05ac, 0x055b, "Gamesir-G3w", 0, XTYPE_XBOX360, QUIRK_360_START},
    {0x05fd, 0x1007, "Mad Catz Controller (unverified)", 0, XTYPE_XBOX},
    {0x05fd, 0x107a, "InterAct 'PowerPad Pro' X-Box pad (Germany)", 0, XTYPE_XBOX},
    {0x05fe, 0x3030, "Chic Controller", 0, XTYPE_XBOX},
    {0x05fe, 0x3031, "Chic Controller", 0, XTYPE_XBOX},
    {0x062a, 0x0020, "Logic3 Xbox GamePad", 0, XTYPE_XBOX},
    {0x062a, 0x0033, "Competition Pro Steering Wheel", 0, XTYPE_XBOX},
    {0x06a3, 0x0200, "Saitek Racing Wheel", 0, XTYPE_XBOX},
    {0x06a3, 0x0201, "Saitek Adrenalin", 0, XTYPE_XBOX},
    {0x06a3, 0xf51a, "Saitek P3600", 0, XTYPE_XBOX360},
    {0x0738, 0x4503, "Mad Catz Racing Wheel", 0, XTYPE_XBOXONE},
    {0x0738, 0x4506, "Mad Catz 4506 Wireless Controller", 0, XTYPE_XBOX},
    {0x0738, 0x4516, "Mad Catz Control Pad", 0, XTYPE_XBOX},
    {0x0738, 0x4520, "Mad Catz Control Pad Pro", 0, XTYPE_XBOX},
    {0x0738, 0x4522, "Mad Catz LumiCON", 0, XTYPE_XBOX},
    {0x0738, 0x4526, "Mad Catz Control Pad Pro", 0, XTYPE_XBOX},
    {0x0738, 0x4530, "Mad Catz Universal MC2 Racing Wheel and Pedals", 0, XTYPE_XBOX},
    {0x0738, 0x4536, "Mad Catz MicroCON", 0, XTYPE_XBOX},
    {0x0738, 0x4540, "Mad Catz Beat Pad", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x0738, 0x4556, "Mad Catz Lynx Wireless Controller", 0, XTYPE_XBOX},
    {0x0738, 0x4586, "Mad Catz MicroCon Wireless Controller", 0, XTYPE_XBOX},
    {0x0738, 0x4588, "Mad Catz Blaster", 0, XTYPE_XBOX},
    {0x0738, 0x45ff, "Mad Catz Beat Pad (w/ Handle)", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x0738, 0x4716, "Mad Catz Wired Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x0738, 0x4718, "Mad Catz Street Fighter IV FightStick SE", 0, XTYPE_XBOX360},
    {0x0738, 0x4726, "Mad Catz Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x0738, 0x4728, "Mad Catz Street Fighter IV FightPad", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0738, 0x4736, "Mad Catz MicroCon Gamepad", 0, XTYPE_XBOX360},
    {0x0738, 0x4738, "Mad Catz Wired Xbox 360 Controller (SFIV)", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0738, 0x4740, "Mad Catz Beat Pad", 0, XTYPE_XBOX360},
    {0x0738, 0x4743, "Mad Catz Beat Pad Pro", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x0738, 0x4758, "Mad Catz Arcade Game Stick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0738, 0x4a01, "Mad Catz FightStick TE 2", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOXONE},
    {0x0738, 0x6040, "Mad Catz Beat Pad Pro", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x0738, 0x9871, "Mad Catz Portable Drum", 0, XTYPE_XBOX360},
    {0x0738, 0xb726, "Mad Catz Xbox controller - MW2", 0, XTYPE_XBOX360},
    {0x0738, 0xb738, "Mad Catz MVC2TE Stick 2", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0738, 0xbeef, "Mad Catz JOYTECH NEO SE Advanced GamePad", XTYPE_XBOX360},
    {0x0738, 0xcb02, "Saitek Cyborg Rumble Pad - PC/Xbox 360", 0, XTYPE_XBOX360},
    {0x0738, 0xcb03, "Saitek P3200 Rumble Pad - PC/Xbox 360", 0, XTYPE_XBOX360},
    {0x0738, 0xcb29, "Saitek Aviator Stick AV8R02", 0, XTYPE_XBOX360},
    {0x0738, 0xf738, "Super SFIV FightStick TE S", 0, XTYPE_XBOX360},
    {0x07ff, 0xffff, "Mad Catz GamePad", 0, XTYPE_XBOX360},
    {0x0c12, 0x0005, "Intec wireless", 0, XTYPE_XBOX},
    {0x0c12, 0x8801, "Nyko Xbox Controller", 0, XTYPE_XBOX},
    {0x0c12, 0x8802, "Zeroplus Xbox Controller", 0, XTYPE_XBOX},
    {0x0c12, 0x8809, "RedOctane Xbox Dance Pad", DANCEPAD_MAP_CONFIG, XTYPE_XBOX},
    {0x0c12, 0x880a, "Pelican Eclipse PL-2023", 0, XTYPE_XBOX},
    {0x0c12, 0x8810, "Zeroplus Xbox Controller", 0, XTYPE_XBOX},
    {0x0c12, 0x9902, "HAMA VibraX - *FAULTY HARDWARE*", 0, XTYPE_XBOX},
    {0x0d2f, 0x0002, "Andamiro Pump It Up pad", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x0e4c, 0x1097, "Radica Gamester Controller", 0, XTYPE_XBOX},
    {0x0e4c, 0x1103, "Radica Gamester Reflex", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX},
    {0x0e4c, 0x2390, "Radica Games Jtech Controller", 0, XTYPE_XBOX},
    {0x0e4c, 0x3510, "Radica Gamester", 0, XTYPE_XBOX},
    {0x0e6f, 0x0003, "Logic3 Freebird wireless Controller", 0, XTYPE_XBOX},
    {0x0e6f, 0x0005, "Eclipse wireless Controller", 0, XTYPE_XBOX},
    {0x0e6f, 0x0006, "Edge wireless Controller", 0, XTYPE_XBOX},
    {0x0e6f, 0x0008, "After Glow Pro Controller", 0, XTYPE_XBOX},
    {0x0e6f, 0x0105, "HSM3 Xbox360 dancepad", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX360},
    {0x0e6f, 0x0113, "Afterglow AX.1 Gamepad for Xbox 360", 0, XTYPE_XBOX360},
    {0x0e6f, 0x011f, "Rock Candy Gamepad Wired Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0131, "PDP EA Sports Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0133, "Xbox 360 Wired Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0139, "Afterglow Prismatic Wired Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x013a, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0146, "Rock Candy Wired Controller for Xbox One", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0147, "PDP Marvel Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x015c, "PDP Xbox One Arcade Stick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOXONE},
    {0x0e6f, 0x0161, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0162, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0163, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0164, "PDP Battlefield One", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0165, "PDP Titanfall 2", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0201, "Pelican PL-3601 'TSZ' Wired Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0213, "Afterglow Gamepad for Xbox 360", 0, XTYPE_XBOX360},
    {0x0e6f, 0x021f, "Rock Candy Gamepad for Xbox 360", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0246, "Rock Candy Gamepad for Xbox One 2015", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a0, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a1, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a2, "PDP Wired Controller for Xbox One - Crimson Red", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a4, "PDP Wired Controller for Xbox One - Stealth Series", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a6, "PDP Wired Controller for Xbox One - Camo Series", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a7, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02a8, "PDP Xbox One Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02ab, "PDP Controller for Xbox One", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02ad, "PDP Wired Controller for Xbox One - Stealth Series", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02b3, "Afterglow Prismatic Wired Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x02b8, "Afterglow Prismatic Wired Controller", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0301, "Logic3 Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0346, "Rock Candy Gamepad for Xbox One 2016", 0, XTYPE_XBOXONE},
    {0x0e6f, 0x0401, "Logic3 Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0413, "Afterglow AX.1 Gamepad for Xbox 360", 0, XTYPE_XBOX360},
    {0x0e6f, 0x0501, "PDP Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x0e6f, 0xf900, "PDP Afterglow AX.1", 0, XTYPE_XBOX360},
    {0x0e8f, 0x0201, "SmartJoy Frag Xpad/PS2 adaptor", 0, XTYPE_XBOX},
    {0x0e8f, 0x3008, "Generic xbox control (dealextreme)", 0, XTYPE_XBOX},
    {0x0f0d, 0x000a, "Hori Co. DOA4 FightStick", 0, XTYPE_XBOX360},
    {0x0f0d, 0x000c, "Hori PadEX Turbo", 0, XTYPE_XBOX360},
    {0x0f0d, 0x000d, "Hori Fighting Stick EX2", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0f0d, 0x0016, "Hori Real Arcade Pro.EX", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0f0d, 0x001b, "Hori Real Arcade Pro VX", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x0f0d, 0x0063, "Hori Real Arcade Pro Hayabusa (USA) Xbox One", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOXONE},
    {0x0f0d, 0x0067, "HORIPAD ONE", 0, XTYPE_XBOXONE},
    {0x0f0d, 0x0078, "Hori Real Arcade Pro V Kai Xbox One", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOXONE},
    {0x0f0d, 0x00c5, "Hori Fighting Commander ONE", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOXONE},
    {0x0f0d, 0x00dc, "HORIPAD FPS for Nintendo Switch", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360, QUIRK_360_START_PKT_1},
    {0x0f30, 0x010b, "Philips Recoil", 0, XTYPE_XBOX},
    {0x0f30, 0x0202, "Joytech Advanced Controller", 0, XTYPE_XBOX},
    {0x0f30, 0x8888, "BigBen XBMiniPad Controller", 0, XTYPE_XBOX},
    {0x102c, 0xff0c, "Joytech Wireless Advanced Controller", 0, XTYPE_XBOX},
    {0x1038, 0x1430, "SteelSeries Stratus Duo", 0, XTYPE_XBOX360},
    {0x1038, 0x1431, "SteelSeries Stratus Duo", 0, XTYPE_XBOX360},
    {0x10f5, 0x7005, "Turtle Beach Recon Controller", 0, XTYPE_XBOXONE},
    {0x11c9, 0x55f0, "Nacon GC-100XF", 0, XTYPE_XBOX360},
    {0x1209, 0x2882, "Ardwiino Controller", 0, XTYPE_XBOX360},
    {0x12ab, 0x0004, "Honey Bee Xbox360 dancepad", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX360},
    {0x12ab, 0x0301, "PDP AFTERGLOW AX.1", 0, XTYPE_XBOX360},
    {0x12ab, 0x0303, "Mortal Kombat Klassic FightStick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x12ab, 0x8809, "Xbox DDR dancepad", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x1430, 0x079B, "RedOctane GHL Controller", 0, XTYPE_XBOXONE, QUIRK_GHL_XBOXONE},
    {0x1430, 0x4748, "RedOctane Guitar Hero X-plorer", 0, XTYPE_XBOX360},
    {0x1430, 0x8888, "TX6500+ Dance Pad (first generation)", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX},
    {0x1430, 0xf801, "RedOctane Controller", 0, XTYPE_XBOX360},
    {0x146b, 0x0601, "BigBen Interactive XBOX 360 Controller", 0, XTYPE_XBOX360},
    {0x146b, 0x0604, "Bigben Interactive DAIJA Arcade Stick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1532, 0x0037, "Razer Sabertooth", 0, XTYPE_XBOX360},
    {0x1532, 0x0a00, "Razer Atrox Arcade Stick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOXONE},
    {0x1532, 0x0a03, "Razer Wildcat", 0, XTYPE_XBOXONE},
    {0x1532, 0x0a29, "Razer Wolverine v2", 0, XTYPE_XBOXONE},
    {0x15e4, 0x3f00, "Power A Mini Pro Elite", 0, XTYPE_XBOX360},
    {0x15e4, 0x3f0a, "Xbox Airflo wired controller", 0, XTYPE_XBOX360},
    {0x15e4, 0x3f10, "Batarang Xbox 360 controller", 0, XTYPE_XBOX360},
    {0x162e, 0xbeef, "Joytech Neo-Se Take2", 0, XTYPE_XBOX360},
    {0x1689, 0xfd00, "Razer Onza Tournament Edition", 0, XTYPE_XBOX360},
    {0x1689, 0xfd01, "Razer Onza Classic Edition", 0, XTYPE_XBOX360},
    {0x1689, 0xfe00, "Razer Sabertooth", 0, XTYPE_XBOX360},
    {0x1949, 0x041a, "Amazon Game Controller", 0, XTYPE_XBOX360},
    {0x1bad, 0x0002, "Harmonix Rock Band Guitar", 0, XTYPE_XBOX360},
    {0x1bad, 0x0003, "Harmonix Rock Band Drumkit", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0x0130, "Ion Drum Rocker", MAP_DPAD_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf016, "Mad Catz Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x1bad, 0xf018, "Mad Catz Street Fighter IV SE Fighting Stick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf019, "Mad Catz Brawlstick for Xbox 360", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf021, "Mad Cats Ghost Recon FS GamePad", 0, XTYPE_XBOX360},
    {0x1bad, 0xf023, "MLG Pro Circuit Controller (Xbox)", 0, XTYPE_XBOX360},
    {0x1bad, 0xf025, "Mad Catz Call Of Duty", 0, XTYPE_XBOX360},
    {0x1bad, 0xf027, "Mad Catz FPS Pro", 0, XTYPE_XBOX360},
    {0x1bad, 0xf028, "Street Fighter IV FightPad", 0, XTYPE_XBOX360},
    {0x1bad, 0xf02e, "Mad Catz Fightpad", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf030, "Mad Catz Xbox 360 MC2 MicroCon Racing Wheel", 0, XTYPE_XBOX360},
    {0x1bad, 0xf036, "Mad Catz MicroCon GamePad Pro", 0, XTYPE_XBOX360},
    {0x1bad, 0xf038, "Street Fighter IV FightStick TE", 0, XTYPE_XBOX360},
    {0x1bad, 0xf039, "Mad Catz MvC2 TE", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf03a, "Mad Catz SFxT Fightstick Pro", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf03d, "Street Fighter IV Arcade Stick TE - Chun Li", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf03e, "Mad Catz MLG FightStick TE", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf03f, "Mad Catz FightStick SoulCaliber", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf042, "Mad Catz FightStick TES+", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf080, "Mad Catz FightStick TE2", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf501, "HoriPad EX2 Turbo", 0, XTYPE_XBOX360},
    {0x1bad, 0xf502, "Hori Real Arcade Pro.VX SA", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf503, "Hori Fighting Stick VX", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf504, "Hori Real Arcade Pro. EX", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf505, "Hori Fighting Stick EX2B", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xf506, "Hori Real Arcade Pro.EX Premium VLX", 0, XTYPE_XBOX360},
    {0x1bad, 0xf900, "Harmonix Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x1bad, 0xf901, "Gamestop Xbox 360 Controller", 0, XTYPE_XBOX360},
    {0x1bad, 0xf903, "Tron Xbox 360 controller", 0, XTYPE_XBOX360},
    {0x1bad, 0xf904, "PDP Versus Fighting Pad", 0, XTYPE_XBOX360},
    {0x1bad, 0xf906, "MortalKombat FightStick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x1bad, 0xfa01, "MadCatz GamePad", 0, XTYPE_XBOX360},
    {0x1bad, 0xfd00, "Razer Onza TE", 0, XTYPE_XBOX360},
    {0x1bad, 0xfd01, "Razer Onza", 0, XTYPE_XBOX360},
    {0x20d6, 0x2001, "BDA Xbox Series X Wired Controller", 0, XTYPE_XBOXONE},
    {0x20d6, 0x2009, "PowerA Enhanced Wired Controller for Xbox Series X|S", 0, XTYPE_XBOXONE},
    {0x20d6, 0x281f, "PowerA Wired Controller For Xbox 360", 0, XTYPE_XBOX360},
    {0x2e24, 0x0652, "Hyperkin Duke X-Box One pad", 0, XTYPE_XBOXONE},
    {0x24c6, 0x5000, "Razer Atrox Arcade Stick", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x24c6, 0x5300, "PowerA MINI PROEX Controller", 0, XTYPE_XBOX360},
    {0x24c6, 0x5303, "Xbox Airflo wired controller", 0, XTYPE_XBOX360},
    {0x24c6, 0x530a, "Xbox 360 Pro EX Controller", 0, XTYPE_XBOX360},
    {0x24c6, 0x531a, "PowerA Pro Ex", 0, XTYPE_XBOX360},
    {0x24c6, 0x5397, "FUS1ON Tournament Controller", 0, XTYPE_XBOX360},
    {0x24c6, 0x541a, "PowerA Xbox One Mini Wired Controller", 0, XTYPE_XBOXONE},
    {0x24c6, 0x542a, "Xbox ONE spectra", 0, XTYPE_XBOXONE},
    {0x24c6, 0x543a, "PowerA Xbox One wired controller", 0, XTYPE_XBOXONE},
    {0x24c6, 0x5500, "Hori XBOX 360 EX 2 with Turbo", 0, XTYPE_XBOX360},
    {0x24c6, 0x5501, "Hori Real Arcade Pro VX-SA", 0, XTYPE_XBOX360},
    {0x24c6, 0x5502, "Hori Fighting Stick VX Alt", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x24c6, 0x5503, "Hori Fighting Edge", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x24c6, 0x5506, "Hori SOULCALIBUR V Stick", 0, XTYPE_XBOX360},
    {0x24c6, 0x5510, "Hori Fighting Commander ONE (Xbox 360/PC Mode)", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x24c6, 0x550d, "Hori GEM Xbox controller", 0, XTYPE_XBOX360},
    {0x24c6, 0x550e, "Hori Real Arcade Pro V Kai 360", MAP_TRIGGERS_TO_BUTTONS, XTYPE_XBOX360},
    {0x24c6, 0x551a, "PowerA FUSION Pro Controller", 0, XTYPE_XBOXONE},
    {0x24c6, 0x561a, "PowerA FUSION Controller", 0, XTYPE_XBOXONE},
    {0x24c6, 0x581a, "ThrustMaster XB1 Classic Controller", 0, XTYPE_XBOXONE},
    {0x24c6, 0x5b00, "ThrustMaster Ferrari 458 Racing Wheel", 0, XTYPE_XBOX360},
    {0x24c6, 0x5b02, "Thrustmaster, Inc. GPX Controller", 0, XTYPE_XBOX360},
    {0x24c6, 0x5b03, "Thrustmaster Ferrari 458 Racing Wheel", 0, XTYPE_XBOX360},
    {0x24c6, 0x5d04, "Razer Sabertooth", 0, XTYPE_XBOX360},
    {0x24c6, 0xfafe, "Rock Candy Gamepad for Xbox 360", 0, XTYPE_XBOX360},
    {0x2563, 0x058d, "OneXPlayer Gamepad", 0, XTYPE_XBOX360},
    {0x2dc8, 0x2000, "8BitDo Pro 2 Wired Controller for Xbox", MAP_SELECT_BUTTON, XTYPE_XBOXONE},
    {0x2dc8, 0x3106, "8BitDo Ultimate Wireless / Pro 2 Wired Controller", 0, XTYPE_XBOX360},
    {0x2dc8, 0x3109, "8BitDo Ultimate Wireless Bluetooth", 0, XTYPE_XBOX360},
    {0x31e3, 0x1100, "Wooting One", 0, XTYPE_XBOX360},
    {0x31e3, 0x1200, "Wooting Two", 0, XTYPE_XBOX360},
    {0x31e3, 0x1210, "Wooting Lekker", 0, XTYPE_XBOX360},
    {0x31e3, 0x1220, "Wooting Two HE", 0, XTYPE_XBOX360},
    {0x31e3, 0x1300, "Wooting 60HE (AVR)", 0, XTYPE_XBOX360},
    {0x31e3, 0x1310, "Wooting 60HE (ARM)", 0, XTYPE_XBOX360},
    {0x3285, 0x0607, "Nacon GC-100", 0, XTYPE_XBOX360},
    {0x3285, 0x0614, "Nacon Pro Compact", 0, XTYPE_XBOXONE},
    {0x3767, 0x0101, "Fanatec Speedster 3 Forceshock Wheel", 0, XTYPE_XBOX},
    {0xffff, 0xffff, "Chinese-made Xbox Controller", 0, XTYPE_XBOX},
    {0x0000, 0x0000, "Generic X-Box pad", 0, XTYPE_UNKNOWN}};

int usb_hid_init(struct hid_device* device);
int usb_hid_get_report(struct hid_device* device, ScePadData* pData);
int usb_hid_write(struct hid_device* device, uint8_t* data, size_t length);
int usb_hid_read(struct hid_device* device, uint8_t* data, size_t length);

#endif
