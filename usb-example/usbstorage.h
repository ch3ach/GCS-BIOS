/* 
 * File:   usbstorage.h
 * Author: gandhi
 *
 * Created on 15. Februar 2013, 08:25
 */

#ifndef USBSTORAGE_H
#define	USBSTORAGE_H

#define MSC_ENDPOINT_PACKAGE_SIZE       64

#ifdef	__cplusplus
extern "C" {
#endif

    extern const struct usb_interface_descriptor msc_iface[];
    
    void msc_data_rx_cb(usbd_device *usbd_dev, u8 ep);


#ifdef	__cplusplus
}
#endif

#endif	/* USBSTORAGE_H */

