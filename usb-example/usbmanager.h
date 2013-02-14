/* 
 * File:   usbmanager.h
 * Author: gandhi
 *
 * Created on 14. Februar 2013, 09:03
 */

#ifndef USBMANAGER_H
#define	USBMANAGER_H

#define USB_CLASS_MISCELLANEOUS                 0xEF
#define USB_CONFIG_POWER_MA(mA)                ((mA)/2)

#define USB_CONFIG_ATTR_POWERED_MASK                0x40
#define USB_CONFIG_ATTR_BUS_POWERED                 0x80
//#define USB_CONFIG_ATTR_SELF_POWERED                0xC0
//#define USB_CONFIG_ATTR_REMOTE_WAKEUP               0x20

#ifdef	__cplusplus
extern "C" {
#endif

    void usbmanager_init(void);
    void usbmanager_poll(void);
    u16 usbmanager_send_packet(u8 addr, const void *buf, u16 len);

#ifdef	__cplusplus
}
#endif

#endif	/* USBMANAGER_H */

