/* 
 * File:   usbstorage.h
 * Author: gandhi
 *
 * Created on 15. Februar 2013, 08:25
 */

#ifndef USBSTORAGE_H
#define	USBSTORAGE_H

#define MSC_ENDPOINT_PACKAGE_SIZE       64

#define MSC_RECEIVING_EP                0x03
#define MSC_SENDING_EP                  (MSC_RECEIVING_EP+0x80)

#define MSC_IMPLEMENTED_LUNS            1

#ifdef	__cplusplus
extern "C" {
#endif

    extern const struct usb_interface_descriptor msc_iface[];

    void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep);
    void msc_stateMachine(usbd_device *usbd_dev);
    void msc_resetLUNs(void);

#ifdef	__cplusplus
}
#endif

#endif	/* USBSTORAGE_H */

