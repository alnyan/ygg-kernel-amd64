#include "sys/amd64/hw/usb/usb.h"

static struct usb_controller *g_hc_list = NULL;

void usb_controller_add(struct usb_controller *hc) {
    hc->next = g_hc_list;
    g_hc_list = hc;
}
