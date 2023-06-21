#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "plugin_common.h"
#include "usb_hid.h"

int (*sceUsbdInit)(void);
int (*sceUsbdExit)(void);
libusb_device_handle* (*sceUsbdOpenDeviceWithVidPid)(uint16_t vendorId, uint16_t productId);
int (*sceUsbdClaimInterface)(libusb_device_handle* deviceHandle, int interfaceNumber);

int (*sceUsbdInterruptTransfer)(libusb_device_handle* deviceHandle, uint8_t endpoint, uint8_t* data, int length,
                                int* transferred, uint32_t timeout);
int (*sceUsbdSetInterfaceAltSetting)(libusb_device_handle* deviceHandle, int interfaceNumber, int alternateSetting);
int (*sceUsbdReleaseInterface)(libusb_device_handle* deviceHandle, int interfaceNumber);

libusb_device* (*sceUsbdGetDevice)(libusb_device_handle* deviceHandle);
ssize_t (*sceUsbdGetDeviceList)(libusb_device*** list);
int (*sceUsbdGetDeviceDescriptor)(libusb_device* device, struct libusb_device_descriptor* desc);
void (*sceUsbdFreeDeviceList)(libusb_device** list, int unrefDevices);
int (*sceUsbdGetActiveConfigDescriptor)(libusb_device* device, struct libusb_config_descriptor** config);
int (*sceUsbdGetConfigDescriptor)(libusb_device* device, uint8_t configIndex, struct libusb_config_descriptor** config);
void (*sceUsbdFreeConfigDescriptor)(struct libusb_config_descriptor* config);
void (*sceUsbdClose)(libusb_device_handle* deviceHandle);
struct libusb_transfer* (*sceUsbdAllocTransfer)(int packets);

void (*sceUsbdFillInterruptTransfer)(struct libusb_transfer* transfer, libusb_device_handle* deviceHandle,
                                     uint8_t endpoint, uint8_t* buf, int length, libusb_transfer_cb_fn callback,
                                     void* userData, uint32_t timeout);
int (*sceUsbdSubmitTransfer)(struct libusb_transfer* transfer);
int (*sceUsbdCancelTransfer)(struct libusb_transfer* transfer);
int (*sceUsbdHandleEvents)(void);
int (*sceUsbdControlTransfer)(libusb_device_handle* deviceHandle, uint8_t bmRequestType, uint8_t bRequest,
                              uint16_t wValue, uint16_t wIndex, uint8_t* data, uint16_t wLength, uint32_t timeout);

int xpad_probe(struct libusb_device_descriptor* desc, struct hid_device* device) {
    for (int i = 0; xpad_device_list[i].idVendor; i++) {
        if ((desc->idVendor) == (xpad_device_list[i].idVendor) &&
            (desc->idProduct) == (xpad_device_list[i].idProduct)) {
            device->idVendor = xpad_device_list[i].idVendor;
            device->idProduct = xpad_device_list[i].idProduct;
            device->mapping = xpad_device_list[i].mapping;
            device->xtype = xpad_device_list[i].xtype;
            device->quirks = xpad_device_list[i].quirks;

            printf("device:%s\n", xpad_device_list[i].name);
            return 1;
        }
    }

    return 0;
}

// https://github.com/libsdl-org/SDL/blob/main/src/hidapi/libusb/hid.c

static void calculate_device_quirks(struct hid_device* dev, unsigned short idVendor, unsigned short idProduct) {
    static const int VENDOR_SONY = 0x054c;
    static const int PRODUCT_PS3_CONTROLLER = 0x0268;
    static const int PRODUCT_NAVIGATION_CONTROLLER = 0x042f;

    if (idVendor == VENDOR_SONY &&
        (idProduct == PRODUCT_PS3_CONTROLLER || idProduct == PRODUCT_NAVIGATION_CONTROLLER)) {
        dev->skip_output_report_id = 1;
        dev->no_output_reports_on_intr_ep = 1;
    }
}

static uint16_t get_report_descriptor_size_from_interface_descriptors(
    const struct libusb_interface_descriptor* intf_desc) {
    int i = 0;
    int found_hid_report_descriptor = 0;
    uint16_t result = 4096;
    const unsigned char* extra = intf_desc->extra;
    int extra_length = intf_desc->extra_length;

    /*
     "extra" contains a HID descriptor
     See section 6.2.1 of HID 1.1 specification.
    */

    while (extra_length >= 2) { /* Descriptor header: bLength/bDescriptorType */
        if (extra[1] == 0x21) { /* bDescriptorType */
            if (extra_length < 6) {
                printf("Broken HID descriptor: not enough data\n");
                break;
            }
            unsigned char bNumDescriptors = extra[5];
            if (extra_length < (6 + 3 * bNumDescriptors)) {
                printf("Broken HID descriptor: not enough data for Report metadata\n");
                break;
            }
            for (i = 0; i < bNumDescriptors; i++) {
                if (extra[6 + 3 * i] == 0x22) {
                    result = (uint16_t)extra[6 + 3 * i + 2] << 8 | extra[6 + 3 * i + 1];
                    found_hid_report_descriptor = 1;
                    break;
                }
            }

            if (!found_hid_report_descriptor) {
                /* We expect to find exactly 1 HID descriptor (LIBUSB_DT_HID)
                   which should contain exactly one HID Report Descriptor metadata (LIBUSB_DT_REPORT). */
                printf("Broken HID descriptor: missing Report descriptor\n");
            }
            break;
        }

        if (extra[0] == 0) { /* bLength */
            printf("Broken HID Interface descriptors: zero-sized descriptor\n");
            break;
        }

        /* Iterate over to the next Descriptor */
        extra_length -= extra[0];
        extra += extra[0];
    }

    return result;
}

static int is_xbox360(unsigned short vendor_id, const struct libusb_interface_descriptor* intf_desc) {
    static const int xb360_iface_subclass = 93;
    static const int xb360_iface_protocol = 1;    /* Wired */
    static const int xb360w_iface_protocol = 129; /* Wireless */
    static const int supported_vendors[] = {
        0x0079, /* GPD Win 2 */
        0x044f, /* Thrustmaster */
        0x045e, /* Microsoft */
        0x046d, /* Logitech */
        0x056e, /* Elecom */
        0x06a3, /* Saitek */
        0x0738, /* Mad Catz */
        0x07ff, /* Mad Catz */
        0x0e6f, /* PDP */
        0x0f0d, /* Hori */
        0x1038, /* SteelSeries */
        0x11c9, /* Nacon */
        0x12ab, /* Unknown */
        0x1430, /* RedOctane */
        0x146b, /* BigBen */
        0x1532, /* Razer Sabertooth */
        0x15e4, /* Numark */
        0x162e, /* Joytech */
        0x1689, /* Razer Onza */
        0x1949, /* Lab126, Inc. */
        0x1bad, /* Harmonix */
        0x20d6, /* PowerA */
        0x24c6, /* PowerA */
        0x2c22, /* Qanba */
        0x2dc8, /* 8BitDo */
        0x9886, /* ASTRO Gaming */
    };

    if (intf_desc->bInterfaceClass == 0xFF && intf_desc->bInterfaceSubClass == xb360_iface_subclass &&
        (intf_desc->bInterfaceProtocol == xb360_iface_protocol ||
         intf_desc->bInterfaceProtocol == xb360w_iface_protocol)) {
        size_t i;
        for (i = 0; i < sizeof(supported_vendors) / sizeof(supported_vendors[0]); ++i) {
            if (vendor_id == supported_vendors[i]) {
                return 1;
            }
        }
    }
    return 0;
}

static int is_xboxone(unsigned short vendor_id, const struct libusb_interface_descriptor* intf_desc) {
    static const int xb1_iface_subclass = 71;
    static const int xb1_iface_protocol = 208;
    static const int supported_vendors[] = {
        0x044f, /* Thrustmaster */
        0x045e, /* Microsoft */
        0x0738, /* Mad Catz */
        0x0e6f, /* PDP */
        0x0f0d, /* Hori */
        0x10f5, /* Turtle Beach */
        0x1532, /* Razer Wildcat */
        0x20d6, /* PowerA */
        0x24c6, /* PowerA */
        0x2dc8, /* 8BitDo */
        0x2e24, /* Hyperkin */
        0x3537, /* GameSir */
    };

    if (intf_desc->bInterfaceNumber == 0 && intf_desc->bInterfaceClass == 0xFF &&
        intf_desc->bInterfaceSubClass == xb1_iface_subclass && intf_desc->bInterfaceProtocol == xb1_iface_protocol) {
        size_t i;
        for (i = 0; i < sizeof(supported_vendors) / sizeof(supported_vendors[0]); ++i) {
            if (vendor_id == supported_vendors[i]) {
                return 1;
            }
        }
    }
    return 0;
}

static int should_enumerate_interface(unsigned short vendor_id, const struct libusb_interface_descriptor* intf_desc) {
    printf("Checking interface 0x%x %d/%d/%d/%d\n", vendor_id, intf_desc->bInterfaceNumber, intf_desc->bInterfaceClass,
           intf_desc->bInterfaceSubClass, intf_desc->bInterfaceProtocol);

    // LIBUSB_CLASS_HID
    if (intf_desc->bInterfaceClass == 3) return 1;

    /* Also enumerate Xbox 360 controllers */
    if (is_xbox360(vendor_id, intf_desc)) return 1;

    /* Also enumerate Xbox One controllers */
    if (is_xboxone(vendor_id, intf_desc)) return 1;

    return 0;
}

static void init_xbox360(libusb_device_handle* device_handle, unsigned short idVendor, unsigned short idProduct,
                         const struct libusb_config_descriptor* conf_desc) {
    (void)conf_desc;

    if ((idVendor == 0x05ac && idProduct == 0x055b) /* Gamesir-G3w */ ||
        idVendor == 0x0f0d /* Hori Xbox controllers */) {
        unsigned char data[20];

        /* The HORIPAD FPS for Nintendo Switch requires this to enable input reports.
           This VID/PID is also shared with other HORI controllers, but they all seem
           to be fine with this as well.
         */
        memset(data, 0, sizeof(data));
        sceUsbdControlTransfer(device_handle, 0xC1, 0x01, 0x100, 0x0, data, sizeof(data), 100);
    }
}

static void init_xboxone(libusb_device_handle* device_handle, unsigned short idVendor, unsigned short idProduct,
                         const struct libusb_config_descriptor* conf_desc) {
    static const int vendor_microsoft = 0x045e;
    static const int xb1_iface_subclass = 71;
    static const int xb1_iface_protocol = 208;
    int j, k, res;

    (void)idProduct;

    for (j = 0; j < conf_desc->bNumInterfaces; j++) {
        const struct libusb_interface* intf = &conf_desc->interface[j];
        for (k = 0; k < intf->num_altsetting; k++) {
            const struct libusb_interface_descriptor* intf_desc = &intf->altsetting[k];
            if (intf_desc->bInterfaceClass == 0xFF && intf_desc->bInterfaceSubClass == xb1_iface_subclass &&
                intf_desc->bInterfaceProtocol == xb1_iface_protocol) {
                int bSetAlternateSetting = 0;

                /* Newer Microsoft Xbox One controllers have a high speed alternate setting */
                if (idVendor == vendor_microsoft && intf_desc->bInterfaceNumber == 0 &&
                    intf_desc->bAlternateSetting == 1) {
                    bSetAlternateSetting = 1;
                } else if (intf_desc->bInterfaceNumber != 0 && intf_desc->bAlternateSetting == 0) {
                    bSetAlternateSetting = 1;
                }

                if (bSetAlternateSetting) {
                    res = sceUsbdClaimInterface(device_handle, intf_desc->bInterfaceNumber);
                    if (res < 0) {
                        printf("can't claim interface %d: %d\n", intf_desc->bInterfaceNumber, res);
                        continue;
                    }

                    printf("Setting alternate setting for VID/PID 0x%x/0x%x interface %d to %d\n", idVendor, idProduct,
                           intf_desc->bInterfaceNumber, intf_desc->bAlternateSetting);

                    res = sceUsbdSetInterfaceAltSetting(device_handle, intf_desc->bInterfaceNumber,
                                                        intf_desc->bAlternateSetting);
                    if (res < 0) {
                        printf("xbox init: can't set alt setting %d: %d\n", intf_desc->bInterfaceNumber, res);
                    }

                    sceUsbdReleaseInterface(device_handle, intf_desc->bInterfaceNumber);
                }
            }
        }
    }
}

/* Helper function, to simplify hid_read().
   This should be called with dev->mutex locked. */
static int return_data(struct hid_device* dev, unsigned char* data, size_t length) {
    /* Copy the data out of the linked list item (rpt) into the
       return buffer (data), and delete the liked list item. */
    struct input_report* rpt = dev->input_reports;
    size_t len = (length < rpt->len) ? length : rpt->len;
    if (len > 0) memcpy(data, rpt->data, len);
    dev->input_reports = rpt->next;
    free(rpt->data);
    free(rpt);
    return len;
}

static void read_callback(struct libusb_transfer* transfer) {
    struct hid_device* dev = transfer->user_data;
    int res;

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED) {
        struct input_report* rpt = (struct input_report*)malloc(sizeof(*rpt));
        rpt->data = (uint8_t*)malloc(transfer->actual_length);
        memcpy(rpt->data, transfer->buffer, transfer->actual_length);
        rpt->len = transfer->actual_length;
        rpt->next = NULL;

        pthread_mutex_lock(&dev->mutex);

        /* Attach the new report object to the end of the list. */
        if (dev->input_reports == NULL) {
            /* The list is empty. Put it at the root. */
            dev->input_reports = rpt;
            pthread_cond_signal(&dev->condition);
        } else {
            /* Find the end of the list and attach. */
            struct input_report* cur = dev->input_reports;
            int num_queued = 0;
            while (cur->next != NULL) {
                cur = cur->next;
                num_queued++;
            }
            cur->next = rpt;

            /* Pop one off if we've reached 30 in the queue. This
               way we don't grow forever if the user never reads
               anything from the device. */
            if (num_queued > 30) {
                return_data(dev, NULL, 0);
            }
        }
        pthread_mutex_unlock(&dev->mutex);

    } else if (transfer->status == LIBUSB_TRANSFER_CANCELLED) {
        dev->shutdown_thread = 1;
    } else if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE) {
        dev->shutdown_thread = 1;
    } else if (transfer->status == LIBUSB_TRANSFER_TIMED_OUT) {
        // LOG("Timeout (normal)\n");
    } else {
        printf("Unknown transfer code: %d\n", transfer->status);
    }

    if (dev->shutdown_thread) {
        dev->transfer_loop_finished = 1;
        return;
    }

    /* Re-submit the transfer object. */
    res = sceUsbdSubmitTransfer(transfer);
    if (res != 0) {
        printf("Unable to submit URB: (%d) \n", res);
        dev->shutdown_thread = 1;
        dev->transfer_loop_finished = 1;
    }
}

static void* read_thread(void* param) {
    int res;
    struct hid_device* dev = param;
    uint8_t* buf;
    const size_t length = dev->input_ep_max_packet_size;

    /* Set up the transfer object. */
    buf = (uint8_t*)malloc(length);

    dev->transfer = sceUsbdAllocTransfer(0);
    sceUsbdFillInterruptTransfer(dev->transfer, dev->handle, dev->input_endpoint, buf, length, read_callback, dev,
                                 5000 /*timeout*/);

    res = sceUsbdSubmitTransfer(dev->transfer);
    if (res < 0) {
        printf("libusb_submit_transfer failed: %d . Stopping read_thread from running\n", res);
        dev->shutdown_thread = 1;
        dev->transfer_loop_finished = 1;
    }

    // pthread_barrier_wait(&dev->barrier);

    /* Handle all the events. */
    while (!dev->shutdown_thread) {
        res = sceUsbdHandleEvents();
        if (res < 0) {
            /* There was an error. */

            /* Break out of this loop only on fatal error.*/
            if (res != 0x80240006 && res != 0x80240007 && res != 0x80240008 && res != 0x8024000A) {
                dev->shutdown_thread = 1;
                break;
            }
        }
    }

    /* Cancel any transfer that may be pending. This call will fail
           if no transfers are pending, but that's OK. */
    sceUsbdCancelTransfer(dev->transfer);
    while (!dev->transfer_loop_finished) {
        //  libusb_handle_events_completed(usb_context, &dev->transfer_loop_finished);
    }
    pthread_mutex_lock(&dev->mutex);
    pthread_cond_broadcast(&dev->condition);
    pthread_mutex_unlock(&dev->mutex);

    return NULL;
}

static int initialize_device(struct hid_device* dev, const struct libusb_interface_descriptor* intf_desc,
                             const struct libusb_config_descriptor* conf_desc) {
    int i = 0;
    int res = 0;
    struct libusb_device_descriptor desc;

    sceUsbdGetDeviceDescriptor(sceUsbdGetDevice(dev->handle), &desc);

    res = sceUsbdClaimInterface(dev->handle, intf_desc->bInterfaceNumber);

    if (res < 0) {
        printf("can't claim interface %d: (%d)\n", intf_desc->bInterfaceNumber, res);
        return 0;
    }

    /* Initialize XBox 360 controllers */
    if (is_xbox360(desc.idVendor, intf_desc)) {
        init_xbox360(dev->handle, desc.idVendor, desc.idProduct, conf_desc);
    }
    //

    /* Initialize XBox One controllers */
    if (is_xboxone(desc.idVendor, intf_desc)) {
        init_xboxone(dev->handle, desc.idVendor, desc.idProduct, conf_desc);
    }

    /* Store off the string descriptor indexes */
    dev->manufacturer_index = desc.iManufacturer;
    dev->product_index = desc.iProduct;
    dev->serial_index = desc.iSerialNumber;

    /* Store off the USB information */
    dev->config_number = conf_desc->bConfigurationValue;
    dev->interface = intf_desc->bInterfaceNumber;
    dev->interface_class = intf_desc->bInterfaceClass;
    dev->interface_subclass = intf_desc->bInterfaceSubClass;
    dev->interface_protocol = intf_desc->bInterfaceProtocol;

    dev->report_descriptor_size = get_report_descriptor_size_from_interface_descriptors(intf_desc);

    dev->input_endpoint = 0;
    dev->input_ep_max_packet_size = 0;
    dev->output_endpoint = 0;

    /* Find the INPUT and OUTPUT endpoints. An
           OUTPUT endpoint is not required. */
    for (i = 0; i < intf_desc->bNumEndpoints; i++) {
        const struct libusb_endpoint_descriptor* ep = &intf_desc->endpoint[i];

        /* Determine the type and direction of this
           endpoint. */
        int is_interrupt = (ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_INTERRUPT;
        int is_output = (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT;
        int is_input = (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN;

        /* Decide whether to use it for input or output. */
        if (dev->input_endpoint == 0 && is_interrupt && is_input) {
            /* Use this endpoint for INPUT */
            dev->input_endpoint = ep->bEndpointAddress;
            dev->input_ep_max_packet_size = ep->wMaxPacketSize;
        }
        if (dev->output_endpoint == 0 && is_interrupt && is_output) {
            /* Use this endpoint for OUTPUT */
            dev->output_endpoint = ep->bEndpointAddress;
        }
    }

    calculate_device_quirks(dev, desc.idVendor, desc.idProduct);

    dev->last_state = (uint8_t*)malloc(USB_PACKET_LENGTH);
    dev->state_buf = (uint8_t*)malloc(USB_PACKET_LENGTH);

    pthread_create(&dev->thread, NULL, read_thread, dev);

    // pthread_barrier_wait(&dev->barrier);
    return 1;
}

int usb_hid_init(struct hid_device* device) {
    char module[256];

    int h = 0;
    int32_t ret = 0;
    int found = 0;
    int good_open = 0;
    libusb_device** list;

    device->blocking = 0;

    pthread_mutex_init(&device->mutex, NULL);
    pthread_cond_init(&device->condition, NULL);
    // pthread_barrier_init(&device->barrier, NULL, 2);

    snprintf(module, 256, "/%s/common/lib/%s", sceKernelGetFsSandboxRandomWord(), "libSceUsbd.sprx");

    sys_dynlib_load_prx(module, &h);
    sys_dynlib_dlsym(h, "sceUsbdInit", &sceUsbdInit);
    sys_dynlib_dlsym(h, "sceUsbdExit", &sceUsbdExit);
    sys_dynlib_dlsym(h, "sceUsbdOpenDeviceWithVidPid", &sceUsbdOpenDeviceWithVidPid);
    sys_dynlib_dlsym(h, "sceUsbdClaimInterface", &sceUsbdClaimInterface);
    sys_dynlib_dlsym(h, "sceUsbdInterruptTransfer", &sceUsbdInterruptTransfer);
    sys_dynlib_dlsym(h, "sceUsbdReleaseInterface", &sceUsbdReleaseInterface);
    sys_dynlib_dlsym(h, "sceUsbdGetDevice", &sceUsbdGetDevice);
    sys_dynlib_dlsym(h, "sceUsbdGetDeviceList", &sceUsbdGetDeviceList);
    sys_dynlib_dlsym(h, "sceUsbdGetDeviceDescriptor", &sceUsbdGetDeviceDescriptor);
    sys_dynlib_dlsym(h, "sceUsbdFreeDeviceList", &sceUsbdFreeDeviceList);
    sys_dynlib_dlsym(h, "sceUsbdGetActiveConfigDescriptor", &sceUsbdGetActiveConfigDescriptor);
    sys_dynlib_dlsym(h, "sceUsbdGetConfigDescriptor", &sceUsbdGetConfigDescriptor);
    sys_dynlib_dlsym(h, "sceUsbdSetInterfaceAltSetting", &sceUsbdSetInterfaceAltSetting);
    sys_dynlib_dlsym(h, "sceUsbdClose", &sceUsbdClose);
    sys_dynlib_dlsym(h, "sceUsbdFreeConfigDescriptor", &sceUsbdFreeConfigDescriptor);
    sys_dynlib_dlsym(h, "sceUsbdAllocTransfer", &sceUsbdAllocTransfer);
    sys_dynlib_dlsym(h, "sceUsbdFillInterruptTransfer", &sceUsbdFillInterruptTransfer);
    sys_dynlib_dlsym(h, "sceUsbdSubmitTransfer", &sceUsbdSubmitTransfer);
    sys_dynlib_dlsym(h, "sceUsbdCancelTransfer", &sceUsbdCancelTransfer);
    sys_dynlib_dlsym(h, "sceUsbdHandleEvents", &sceUsbdHandleEvents);
    sys_dynlib_dlsym(h, "sceUsbdControlTransfer", &sceUsbdControlTransfer);
    ret = sceUsbdInit();
    if (ret) {
        printf("sceUsbdInit failed\n");
        return 0;
    }

    int count = sceUsbdGetDeviceList(&list);
    // usb_hid_handle = sceUsbdOpenDeviceWithVidPid(0x45e, 0x0b12);
    printf("Device list count %d\n", count);

    for (int i = 0; i < count; i++) {
        struct libusb_device_descriptor desc;
        ret = sceUsbdGetDeviceDescriptor(list[i], &desc);

        if (xpad_probe(&desc, device)) {
            found = 1;
            break;
        }
    }

    sceUsbdFreeDeviceList(list, 1);

    if (!found) {
        printf("not found xpad device\n");
        return -1;
    }

    device->handle = sceUsbdOpenDeviceWithVidPid(device->idVendor, device->idProduct);

    struct libusb_config_descriptor* conf_desc = NULL;
    ret = sceUsbdGetActiveConfigDescriptor(sceUsbdGetDevice(device->handle), &conf_desc);
    if (ret < 0) {
        sceUsbdGetConfigDescriptor(sceUsbdGetDevice(device->handle), 0, &conf_desc);
    }

    for (int j = 0; j < conf_desc->bNumInterfaces && !good_open; j++) {
        const struct libusb_interface* intf = &conf_desc->interface[j];
        for (int k = 0; k < intf->num_altsetting && !good_open; k++) {
            const struct libusb_interface_descriptor* intf_desc = &intf->altsetting[k];
            if (should_enumerate_interface(device->idVendor, intf_desc)) {
                good_open = initialize_device(device, intf_desc, conf_desc);
                if (!good_open) {
                    sceUsbdClose(device->handle);
                }
            }
        }
    }
    sceUsbdFreeConfigDescriptor(conf_desc);

    // printf("input_endpoint:%02x input_ep_max_packet_size:%d\n", device->input_endpoint,
    //      device->input_ep_max_packet_size);
    if (good_open) {
        if (device->xtype == XTYPE_XBOXONE) {
            uint8_t sequence = 1;
            /* Start controller */
            uint8_t xboxone_init0[] = {0x05, 0x20, 0x03, 0x01, 0x00};

            /* Enable LED */
            uint8_t xboxone_init1[] = {0x0A, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14};

            uint8_t security_passed_packet[] = {0x06, 0x20, 0x00, 0x02, 0x01, 0x00};

            uint8_t xboxone_powera_rumble_init[] = {0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00,
                                                    0x00, 0x1D, 0x1D, 0xFF, 0x00, 0xFF};

            uint8_t xboxone_powera_rumble_init_end[] = {0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00,
                                                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            xboxone_init0[2] = sequence++;
            usb_hid_write(device, xboxone_init0, sizeof(xboxone_init0));

            xboxone_init1[2] = sequence++;
            usb_hid_write(device, xboxone_init1, sizeof(xboxone_init1));

            security_passed_packet[2] = sequence++;
            usb_hid_write(device, security_passed_packet, sizeof(security_passed_packet));

            xboxone_powera_rumble_init[2] = sequence++;
            usb_hid_write(device, xboxone_powera_rumble_init, sizeof(xboxone_powera_rumble_init));

            xboxone_powera_rumble_init_end[2] = sequence++;
            usb_hid_write(device, xboxone_powera_rumble_init_end, sizeof(xboxone_powera_rumble_init_end));

        } else if (device->xtype == XTYPE_XBOX360W) {
            uint8_t init_packet[] = {0x08, 0x00, 0x0F, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
            usb_hid_write(device, init_packet, sizeof(init_packet));
        }

        return 0;
    } else {
        return -1;
    }
}

int usb_hid_exit(struct hid_device* dev) {
    if (!dev) return 0;

    dev->shutdown_thread = 1;
    // libusb_cancel_transfer(dev->transfer);

    sceUsbdExit();
    return 0;
}

static void cleanup_mutex(void* param) {
    struct hid_device* dev = param;
    pthread_mutex_unlock(&dev->mutex);
}

int usb_hid_read_timeout(struct hid_device* dev, uint8_t* data, size_t length, int milliseconds) {
#if 0
    int transferred;
    int res = sceUsbdInterruptTransfer(dev->handle, dev->input_endpoint, data, length, &transferred, 5000);
    printf("transferred: %d\n", transferred);
    return transferred;
#endif

    int bytes_read; /* = -1; */
    pthread_mutex_lock(&dev->mutex);
    pthread_cleanup_push(cleanup_mutex, dev);

    bytes_read = -1;

    /* There's an input report queued up. Return it. */
    if (dev->input_reports) {
        /* Return the first one */
        bytes_read = return_data(dev, data, length);
        goto ret;
    }

    if (dev->shutdown_thread) {
        /* This means the device has been disconnected.
           An error code of -1 should be returned. */
        bytes_read = -1;
        goto ret;
    }

    if (milliseconds == -1) {
        /* Blocking */
        while (!dev->input_reports && !dev->shutdown_thread) {
            pthread_cond_wait(&dev->condition, &dev->mutex);
        }
        if (dev->input_reports) {
            bytes_read = return_data(dev, data, length);
        }
    } else if (milliseconds > 0) {
        /* Non-blocking, but called with timeout. */
        int res;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        //        hidapi_thread_addtime(&ts, milliseconds);
        ts.tv_sec += milliseconds / 1000;
        ts.tv_nsec += (milliseconds % 1000) * 1000000;
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec++;
            ts.tv_nsec -= 1000000000L;
        }

        while (!dev->input_reports && !dev->shutdown_thread) {
            res = pthread_cond_timedwait(&dev->condition, &dev->mutex, &ts);
            ;
            if (res == 0) {
                if (dev->input_reports) {
                    bytes_read = return_data(dev, data, length);
                    break;
                }

                /* If we're here, there was a spurious wake up
                   or the read thread was shutdown. Run the
                   loop again (ie: don't break). */
            } else if (res == ETIMEDOUT) {
                /* Timed out. */
                bytes_read = 0;
                break;
            } else {
                /* Error. */
                bytes_read = -1;
                break;
            }
        }
    } else {
        /* Purely non-blocking */
        bytes_read = 0;
    }

ret:
    pthread_mutex_unlock(&dev->mutex);
    pthread_cleanup_pop(0);

    return bytes_read;
}

int usb_hid_read(struct hid_device* dev, uint8_t* data, size_t length) {
    return usb_hid_read_timeout(dev, data, length, dev->blocking ? -1 : 0);
}

int usb_hid_write(struct hid_device* dev, const uint8_t* data, size_t length) {
    int res;
    int report_number;
    int skipped_report_id = 0;

    if (!data || (length == 0)) {
        return -1;
    }

    report_number = data[0];

    if (report_number == 0x0 || dev->skip_output_report_id) {
        data++;
        length--;
        skipped_report_id = 1;
    }

    if (dev->output_endpoint <= 0 || dev->no_output_reports_on_intr_ep) {
        /* No interrupt out endpoint. Use the Control Endpoint */
        res = sceUsbdControlTransfer(dev->handle,
                                     LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
                                     0x09 /*HID Set_Report*/, (2 /*HID output*/ << 8) | report_number, dev->interface,
                                     (unsigned char*)data, (uint16_t)length, 1000 /*timeout millis*/);

        if (res < 0) return -1;

        if (skipped_report_id) length++;

        return length;
    } else {
        /* Use the interrupt out endpoint */
        int actual_length;
        res = sceUsbdInterruptTransfer(dev->handle, dev->output_endpoint, (unsigned char*)data, length, &actual_length,
                                       1000);

        if (res < 0) return -1;

        if (skipped_report_id) actual_length++;

        return actual_length;
    }
}

void xpad360_process_packet(uint8_t* data, ScePadData* pData) {
    pData->buttons |= (data[2] & 0x01) ? SCE_PAD_BUTTON_UP : 0;
    pData->buttons |= (data[2] & 0x02) ? SCE_PAD_BUTTON_DOWN : 0;
    pData->buttons |= (data[2] & 0x04) ? SCE_PAD_BUTTON_LEFT : 0;
    pData->buttons |= (data[2] & 0x08) ? SCE_PAD_BUTTON_RIGHT : 0;
    pData->buttons |= (data[2] & 0x10) ? SCE_PAD_BUTTON_OPTIONS : 0;
    pData->buttons |= (data[2] & 0x20) ? SCE_PAD_BUTTON_TOUCH_PAD : 0;
    pData->buttons |= (data[2] & 0x40) ? SCE_PAD_BUTTON_L3 : 0;
    pData->buttons |= (data[2] & 0x80) ? SCE_PAD_BUTTON_R3 : 0;

    pData->buttons |= (data[3] & 0x01) ? SCE_PAD_BUTTON_R1 : 0;
    pData->buttons |= (data[3] & 0x02) ? SCE_PAD_BUTTON_L1 : 0;
    pData->buttons |= (data[3] & 0x08) ? SCE_PAD_BUTTON_CROSS : 0;
    pData->buttons |= (data[3] & 0x10) ? SCE_PAD_BUTTON_CIRCLE : 0;
    pData->buttons |= (data[3] & 0x20) ? SCE_PAD_BUTTON_SQUARE : 0;
    pData->buttons |= (data[3] & 0x40) ? SCE_PAD_BUTTON_TRIANGLE : 0;

    int16_t axis = data[4];
    if (axis > 64) {
        pData->buttons |= SCE_PAD_BUTTON_L2;
    }
    pData->analogButtons.l2 = axis;

    axis = data[5];
    if (axis > 64) {
        pData->buttons |= SCE_PAD_BUTTON_R2;
    }
    pData->analogButtons.r2 = axis;

    axis = *(int16_t*)(&data[6]);
    pData->leftStick.x = (axis / 256) + 0x80;

    axis = (*(int16_t*)(&data[8]));
    pData->leftStick.y = (~axis / 256) + 0x80;

    axis = (*(int16_t*)(&data[10]));
    pData->rightStick.x = (axis / 256) + 0x80;

    axis = (*(int16_t*)(&data[12]));
    pData->rightStick.y = (~axis / 256) + 0x80;
}

void xpadone_process_packet(uint8_t* data, ScePadData* pData) {
    // int size = ((4 + data[3]) < XPAD_PKT_LEN) ? (4 + data[3]) : XPAD_PKT_LEN;

    pData->buttons |= (data[4] & 0x04) ? SCE_PAD_BUTTON_OPTIONS : 0;
    pData->buttons |= (data[4] & 0x08) ? SCE_PAD_BUTTON_TOUCH_PAD : 0;
    pData->buttons |= (data[4] & 0x10) ? SCE_PAD_BUTTON_CROSS : 0;
    pData->buttons |= (data[4] & 0x20) ? SCE_PAD_BUTTON_CIRCLE : 0;
    pData->buttons |= (data[4] & 0x40) ? SCE_PAD_BUTTON_SQUARE : 0;
    pData->buttons |= (data[4] & 0x80) ? SCE_PAD_BUTTON_TRIANGLE : 0;

    pData->buttons |= (data[5] & 0x01) ? SCE_PAD_BUTTON_UP : 0;
    pData->buttons |= (data[5] & 0x02) ? SCE_PAD_BUTTON_DOWN : 0;
    pData->buttons |= (data[5] & 0x04) ? SCE_PAD_BUTTON_LEFT : 0;
    pData->buttons |= (data[5] & 0x08) ? SCE_PAD_BUTTON_RIGHT : 0;

    pData->buttons |= (data[5] & 0x10) ? SCE_PAD_BUTTON_L1 : 0;
    pData->buttons |= (data[5] & 0x20) ? SCE_PAD_BUTTON_R1 : 0;
    pData->buttons |= (data[5] & 0x40) ? SCE_PAD_BUTTON_L3 : 0;
    pData->buttons |= (data[5] & 0x80) ? SCE_PAD_BUTTON_R3 : 0;
    // printf("button :%08x data [4]:%02x [5]:%02x\n", pData->buttons, data[4], data[5]);

    // 0~1024
    int16_t axis = *(int16_t*)(&data[6]);

    if (axis > 64) {
        pData->buttons |= SCE_PAD_BUTTON_L2;
    }
    pData->analogButtons.l2 = axis / 4;

    axis = *(int16_t*)(&data[8]);

    if (axis > 64) {
        pData->buttons |= SCE_PAD_BUTTON_R2;
    }

    pData->analogButtons.r2 = axis / 4;

    // x -32768 ~ 32767
    axis = (*(int16_t*)(&data[10]));
    pData->leftStick.x = (axis / 256) + 0x80;

    // y 32767 ~ -32768
    // printf("axis x:%d", axis);
    axis = (*(int16_t*)(&data[12]));

    pData->leftStick.y = (~axis / 256) + 0x80;

    // printf("axis y:%d\n", axis);

    axis = (*(int16_t*)(&data[14]));
    pData->rightStick.x = (axis / 256) + 0x80;

    axis = (*(int16_t*)(&data[16]));
    pData->rightStick.y = (~axis / 256) + 0x80;

    // pData->connected = 1;
    // pData->connectedCount = 1;
    // pData->timestamp = sceKernelGetProcessTime();
}

// int counter = 0;
int usb_hid_get_report(struct hid_device* dev, ScePadData* pData) {
    if (!dev->handle) return -1;

    pData->buttons = 0;
    pData->connected = 1;
    pData->connectedCount = 1;
    pData->timestamp = sceKernelGetProcessTime();

    memset(dev->state_buf, 0, USB_PACKET_LENGTH);
    int32_t actual = usb_hid_read(dev, dev->state_buf, USB_PACKET_LENGTH);
    if (dev->xtype == XTYPE_XBOXONE) {
        if (actual && dev->state_buf[0] == 0x20) {
            memcpy(dev->last_state, dev->state_buf, USB_PACKET_LENGTH);
            xpadone_process_packet(dev->state_buf, pData);
        } else {
            xpadone_process_packet(dev->last_state, pData);
        }
    } else if (dev->xtype == XTYPE_XBOX360) {
        if (actual && dev->state_buf[0] == 0x00) {
            memcpy(dev->last_state, dev->state_buf, USB_PACKET_LENGTH);
            xpad360_process_packet(dev->state_buf, pData);
        } else {
            xpad360_process_packet(dev->last_state, pData);
        }
    } else if (dev->xtype == XTYPE_XBOX360W) {
        if (actual == 29 && dev->state_buf[0] == 0x00 && (dev->state_buf[1] & 0x01) == 0x01) {
            xpad360_process_packet(dev->state_buf, pData);
        } else {
            xpad360_process_packet(dev->last_state, pData);
        }
    }
    return 0;
}

typedef enum {
    XBOX_ONE_RUMBLE_STATE_IDLE,
    XBOX_ONE_RUMBLE_STATE_QUEUED,
    XBOX_ONE_RUMBLE_STATE_BUSY
} SDL_XboxOneRumbleState;

int usb_hid_send_rumble(struct hid_device* device, const ScePadVibrationParam* pParam) {
    uint8_t rumble_packet[13];
    int32_t len = 0;

    if (pParam->largeMotor == 0 && pParam->smallMotor == 0) {
        return 0;
    }

    if (device->rumble_state == XBOX_ONE_RUMBLE_STATE_QUEUED) {
        if (device->rumble_time) {
            device->rumble_state = XBOX_ONE_RUMBLE_STATE_BUSY;
        }
    }

    if (device->rumble_state == XBOX_ONE_RUMBLE_STATE_BUSY) {
        const int RUMBLE_BUSY_TIME = 3000000;
        if (sceKernelGetProcessTime() >= (device->rumble_time + RUMBLE_BUSY_TIME)) {
            device->rumble_time = 0;
            device->rumble_state = XBOX_ONE_RUMBLE_STATE_IDLE;
        }
    }

    if (device->rumble_state != XBOX_ONE_RUMBLE_STATE_IDLE) {
        return 0;
    }

    switch (device->xtype) {
        case XTYPE_XBOX360:
            rumble_packet[0] = 0x00;
            rumble_packet[1] = 0x08;
            rumble_packet[2] = 0x00;
            rumble_packet[3] = pParam->largeMotor; /* left actuator? */
            rumble_packet[4] = pParam->smallMotor; /* right actuator? */
            rumble_packet[5] = 0x00;
            rumble_packet[6] = 0x00;
            rumble_packet[7] = 0x00;
            len = 8;

            break;

        case XTYPE_XBOX360W:
            rumble_packet[0] = 0x00;
            rumble_packet[1] = 0x01;
            rumble_packet[2] = 0x0F;
            rumble_packet[3] = 0xC0;
            rumble_packet[4] = 0x00;
            rumble_packet[5] = pParam->largeMotor;
            rumble_packet[6] = pParam->smallMotor;
            rumble_packet[7] = 0x00;
            rumble_packet[8] = 0x00;
            rumble_packet[9] = 0x00;
            rumble_packet[10] = 0x00;
            rumble_packet[11] = 0x00;
            len = 12;

            // https://github.com/quantus/xbox-one-controller-protocol
        case XTYPE_XBOXONE:
            rumble_packet[0] = 0x09; /* activate rumble */
            rumble_packet[1] = 0x00;
            rumble_packet[2] = device->odata_serial++;
            rumble_packet[3] = 0x09;
            rumble_packet[4] = 0x00;
            rumble_packet[5] = 0x0F;
            rumble_packet[6] = 0x00;                     /* left trigger */
            rumble_packet[7] = 0x00;                     /* right trigger */
            rumble_packet[8] = pParam->largeMotor / 2.6; /* left actuator */
            rumble_packet[9] = pParam->smallMotor / 2.6; /* right actuator */
            rumble_packet[10] = 0x30;                    /* on period */
            rumble_packet[11] = 0x00;                    /* off period */
            rumble_packet[12] = 0x01;                    /* repeat count */
            len = 13;

        default:
            break;
    }

    device->rumble_time = sceKernelGetProcessTime();

    printf("send rumble :%ld largeMotor:%d smallMotor:%d\n", device->rumble_time, pParam->largeMotor,
           pParam->smallMotor);
    usb_hid_write(device, rumble_packet, len);

    device->rumble_state = XBOX_ONE_RUMBLE_STATE_QUEUED;

    return 0;
}
