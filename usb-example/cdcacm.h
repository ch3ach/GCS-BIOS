/* 
 * File:   cdcacm.h
 * Author: gandhi
 *
 * Created on 7. Februar 2013, 18:49
 */

#ifndef CDCACM_H
#define	CDCACM_H

#include <libopencm3/usb/usbd.h>

#define USB_CDC_REQ_SEND_ENCAPSULATED_COMMAND               0x00
#define USB_CDC_REQ_GET_ENCAPSULATED_RESPONSE               0x01
#define USB_CDC_REQ_SET_COMM_FEATURE                        0x02
#define USB_CDC_REQ_GET_COMM_FEATURE                        0x03
#define USB_CDC_REQ_CLEAR_COMM_FEATURE                      0x04
//#define USB_CDC_REQ_SET_LINE_CODING                         0x20
#define USB_CDC_REQ_GET_LINE_CODING                         0x21
//#define USB_CDC_REQ_SET_CONTROL_LINE_STATE                  0x22
#define USB_CDC_REQ_SEND_BREAK                              0x23
#define USB_CDC_REQ_NO_CMD                                  0xFF

#define CDC_ENDPOINT_PACKAGE_SIZE                           512

#define USB_CDC_RECV_BUFFER_COUNT                           2
#define USB_CDC_RECV_BUFFER_SIZE                            64

#ifdef	__cplusplus
extern "C" {
#endif

    extern char recvBuf[USB_CDC_RECV_BUFFER_COUNT][USB_CDC_RECV_BUFFER_SIZE];
    extern volatile int32_t recvBufLen[USB_CDC_RECV_BUFFER_COUNT];
    extern volatile int32_t readBuffer;

    extern const struct usb_interface_descriptor comm_iface[];
    extern const struct usb_interface_descriptor data_iface[];

    void cdcacm_data_rx_cb(usbd_device *usbd_dev, u8 ep);


#ifdef	__cplusplus
}
#endif

#endif	/* CDCACM_H */

