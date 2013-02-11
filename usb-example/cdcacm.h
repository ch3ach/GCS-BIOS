/* 
 * File:   cdcacm.h
 * Author: gandhi
 *
 * Created on 7. Februar 2013, 18:49
 */

#ifndef CDCACM_H
#define	CDCACM_H

#define USB_CLASS_MISCELLANEOUS                 0xEF
#define USB_CONFIG_POWER_MA(mA)                ((mA)/2)

#define USB_CONFIG_ATTR_POWERED_MASK                0x40
#define USB_CONFIG_ATTR_BUS_POWERED                 0x80
//#define USB_CONFIG_ATTR_SELF_POWERED                0xC0
//#define USB_CONFIG_ATTR_REMOTE_WAKEUP               0x20

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

    void cdcacm_set_config(usbd_device *usbd_dev, u16 wValue);


#ifdef	__cplusplus
}
#endif

#endif	/* CDCACM_H */

