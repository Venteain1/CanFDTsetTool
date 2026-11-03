/* USB component (basic PCD init) */
#ifndef APPLICATIONS_USB_H_
#define APPLICATIONS_USB_H_

#include "stm32g4xx_hal.h"
#include <rtthread.h>
#include "usb_tiny_cdc.h"

void usb_component_init(void);

#endif /* APPLICATIONS_USB_H_ */
