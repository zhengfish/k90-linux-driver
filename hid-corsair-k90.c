/*
 * HID driver for Corsair Vengeance K90 Keyboard
 * Copyright (c) 2015 Clément Vuchener
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/module.h>
#include <linux/usb.h>

#define USB_VENDOR_ID_CORSAIR     0x1b1c
#define USB_DEVICE_ID_CORSAIR_K90 0x1b02

struct k90_drvdata {
	int brightness;
	int current_profile;
	int macro_mode;
	int macro_record;
	int meta_locked;
};

#define K90_GKEY_COUNT	18

static int k90_usage_to_gkey(unsigned int usage) {
	/* G1 (0xd0) to G16 (0xdf) */
	if (usage >= 0xd0 && usage <= 0xdf) {
		return usage - 0xd0 + 1;
	}
	/* G17 (0xe8) to G18 (0xe9) */
	if (usage >= 0xe8 && usage <= 0xe9) {
		return usage - 0xe8 + 17;
	}
	return 0;
}

static unsigned short k90_gkey_map[K90_GKEY_COUNT] = {
	KEY_F13,
	KEY_F14,
	KEY_F15,
	KEY_F16,
	KEY_F17,
	KEY_F18,
	KEY_F19,
	KEY_F20,
	KEY_F21,
	KEY_F22,
	KEY_F23,
	KEY_F24,
	BTN_MISC+0,
	BTN_MISC+1,
	BTN_MISC+2,
	BTN_MISC+3,
	BTN_MISC+4,
	BTN_MISC+5,
};
module_param_array_named(gkey_codes, k90_gkey_map, ushort, NULL, S_IRUGO);

#define K90_USAGE_SPECIAL_MIN 0xf0
#define K90_USAGE_SPECIAL_MAX 0xff

#define K90_USAGE_MACRO_RECORD_START 0xf6
#define K90_USAGE_MACRO_RECORD_STOP 0xf7

#define K90_USAGE_PROFILE 0xf1
#define K90_USAGE_M1 0xf1
#define K90_USAGE_M2 0xf2
#define K90_USAGE_M3 0xf3
#define K90_USAGE_PROFILE_MAX 0xf3

#define K90_USAGE_META_OFF 0xf4
#define K90_USAGE_META_ON  0xf5

#define K90_USAGE_LIGHT 0xfa
#define K90_USAGE_LIGHT_OFF 0xfa
#define K90_USAGE_LIGHT_DIM 0xfb
#define K90_USAGE_LIGHT_MEDIUM 0xfc
#define K90_USAGE_LIGHT_BRIGHT 0xfd
#define K90_USAGE_LIGHT_MAX 0xfd

/* USB control protocol */

#define K90_REQUEST_BRIGHTNESS 49
#define K90_REQUEST_MACRO_MODE 2
#define K90_REQUEST_STATUS 4
#define K90_REQUEST_PROFILE 20

#define K90_MACRO_MODE_SW 0x0030
#define K90_MACRO_MODE_HW 0x0001

#define K90_MACRO_LED_ON  0x0020
#define K90_MACRO_LED_OFF 0x0040

static ssize_t k90_show_brightness(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	if (!drvdata) {
		return -ENOSYS;
	}
	return snprintf (buf, PAGE_SIZE, "%d\n", drvdata->brightness);
}

static ssize_t k90_store_brightness(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct usb_interface *usbif = to_usb_interface(dev->parent);
	struct usb_device *usbdev = interface_to_usbdev(usbif);
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	int brightness;

	if (kstrtoint (buf, 10, &brightness)) {
		return -EINVAL;
	}
	if (brightness < 0 || brightness > 3) {
		return -EINVAL;
	}

	if (0 != (ret = usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0),
			K90_REQUEST_BRIGHTNESS,
			USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_DEVICE,
			brightness, 0, NULL, 0, USB_CTRL_SET_TIMEOUT))) {
		return ret;
	}
	drvdata->brightness = brightness;

	return count;
}

static ssize_t k90_show_macro_mode(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	if (!drvdata) {
		return -ENOSYS;
	}
	return snprintf (buf, PAGE_SIZE, "%s\n", (drvdata->macro_mode ? "HW" : "SW"));
}

static ssize_t k90_store_macro_mode(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct usb_interface *usbif = to_usb_interface(dev->parent);
	struct usb_device *usbdev = interface_to_usbdev(usbif);
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	__u16 value;
	
	if (strncmp(buf, "SW", 2) == 0) {
		value = K90_MACRO_MODE_SW;
	}
	else if (strncmp(buf, "HW", 2) == 0) {
		value = K90_MACRO_MODE_HW;
	}
	else {
		return -EINVAL;
	}

	if (0 != (ret = usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0),
			K90_REQUEST_MACRO_MODE,
			USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_DEVICE,
			value, 0, NULL, 0, USB_CTRL_SET_TIMEOUT))) {
		return ret;
	}
	drvdata->macro_mode = (value == K90_MACRO_MODE_HW);

	return count;
}

static ssize_t k90_show_macro_record(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	if (!drvdata) {
		return -ENOSYS;
	}
	return snprintf (buf, PAGE_SIZE, "%s\n", (drvdata->macro_record ? "ON" : "OFF"));
}

static ssize_t k90_store_macro_record(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct usb_interface *usbif = to_usb_interface(dev->parent);
	struct usb_device *usbdev = interface_to_usbdev(usbif);
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	__u16 value;
	
	if (strncmp(buf, "ON", 2) == 0) {
		value = K90_MACRO_LED_ON;
	}
	else if (strncmp(buf, "OFF", 3) == 0) {
		value = K90_MACRO_LED_OFF;
	}
	else {
		return -EINVAL;
	}

	if (0 != (ret = usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0),
			K90_REQUEST_MACRO_MODE,
			USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_DEVICE,
			value, 0, NULL, 0, USB_CTRL_SET_TIMEOUT))) {
		return ret;
	}
	drvdata->macro_record = (value == K90_MACRO_LED_ON);

	return count;
}

static ssize_t k90_show_current_profile(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	if (!drvdata) {
		return -ENOSYS;
	}
	return snprintf (buf, PAGE_SIZE, "%d\n", drvdata->current_profile);
}

static ssize_t k90_store_current_profile(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct usb_interface *usbif = to_usb_interface(dev->parent);
	struct usb_device *usbdev = interface_to_usbdev(usbif);
	struct k90_drvdata *drvdata = dev_get_drvdata (dev);
	int profile;

	if (kstrtoint(buf, 10, &profile)) {
		return -EINVAL;
	}
	if (profile < 1 || profile > 3) {
		return -EINVAL;
	}

	if (0 != (ret = usb_control_msg(usbdev, usb_sndctrlpipe(usbdev, 0),
			K90_REQUEST_PROFILE,
			USB_DIR_OUT|USB_TYPE_VENDOR|USB_RECIP_DEVICE,
			profile, 0, NULL, 0, USB_CTRL_SET_TIMEOUT))) {
		return ret;
	}
	drvdata->current_profile = profile;

	return count;
}

static DEVICE_ATTR(brightness, 0644, k90_show_brightness, k90_store_brightness);
static DEVICE_ATTR(macro_mode, 0644, k90_show_macro_mode, k90_store_macro_mode);
static DEVICE_ATTR(macro_record, 0644, k90_show_macro_record, k90_store_macro_record);
static DEVICE_ATTR(current_profile, 0644, k90_show_current_profile, k90_store_current_profile);

static struct attribute *k90_attrs[] = {
	&dev_attr_brightness.attr,
	&dev_attr_macro_mode.attr,
	&dev_attr_macro_record.attr,
	&dev_attr_current_profile.attr,
	NULL,
};

static const struct attribute_group k90_attr_group = {
	.attrs = k90_attrs,
};

static int k90_init_special_functions(struct hid_device *dev)
{
	int ret;
	struct usb_interface *usbif = to_usb_interface(dev->dev.parent);
	struct usb_device *usbdev = interface_to_usbdev(usbif);
	char data[8];

	if (usbif->cur_altsetting->desc.bInterfaceNumber == 0) {
		struct k90_drvdata *drvdata = kzalloc (sizeof (struct k90_drvdata), GFP_KERNEL);

		if (!drvdata) {
			return -ENOMEM;
		}

		if ((ret = usb_control_msg(usbdev, usb_rcvctrlpipe(usbdev, 0),
				K90_REQUEST_STATUS,
				USB_DIR_IN|USB_TYPE_VENDOR|USB_RECIP_DEVICE,
				0, 0, data, 8, USB_CTRL_SET_TIMEOUT)) < 0) {
			printk (KERN_WARNING "Failed to get K90 initial state (error %d).\n", ret);
			drvdata->brightness = 0;
			drvdata->current_profile = 1;
		}
		else {
			drvdata->brightness = data[4];
			drvdata->current_profile = data[7];
		}
		hid_set_drvdata (dev, drvdata);

		if (0 != (ret = sysfs_create_group(&dev->dev.kobj, &k90_attr_group))) {
			kfree (drvdata);
			hid_set_drvdata (dev, NULL);
			return ret;
		}
	}
	else {
		hid_set_drvdata (dev, NULL);
	}

	return 0;
}

static void k90_cleanup_special_functions(struct hid_device *dev)
{
	struct k90_drvdata *drvdata = hid_get_drvdata (dev);

	if (drvdata) {
		sysfs_remove_group(&dev->dev.kobj, &k90_attr_group);
		kfree (drvdata);
	}
}

static int k90_probe(struct hid_device *dev, const struct hid_device_id *id)
{
	int ret;

	if (0 != (ret = hid_parse(dev))) {
		hid_err(dev, "parse failed\n");
		return ret;
	}
	if (0 != (ret = hid_hw_start(dev, HID_CONNECT_DEFAULT))) {
		hid_err(dev, "hw start failed\n");
		return ret;
	}

	if (0 != (ret = k90_init_special_functions(dev))) {
		printk (KERN_WARNING "Failed to initialize K90 special functions.\n");
		hid_hw_stop(dev);
		return ret;
	}

	return 0;
}

static void k90_remove(struct hid_device *dev)
{
	k90_cleanup_special_functions(dev);
	hid_hw_stop(dev);
}

static int k90_event(struct hid_device *dev, struct hid_field *field,
		struct hid_usage *usage, __s32 value)
{
	struct k90_drvdata* drvdata = hid_get_drvdata (dev);

	if (!drvdata)
		return 0;
	
	switch (usage->hid & HID_USAGE) {
	case K90_USAGE_MACRO_RECORD_START:
		drvdata->macro_record = 1;
		break;
	case K90_USAGE_MACRO_RECORD_STOP:
		drvdata->macro_record = 0;
		break;
	case K90_USAGE_M1:
	case K90_USAGE_M2:
	case K90_USAGE_M3:
		drvdata->current_profile = (usage->hid & HID_USAGE) - K90_USAGE_PROFILE + 1;
		break;
	case K90_USAGE_META_OFF:
		drvdata->meta_locked = 0;
		break;
	case K90_USAGE_META_ON:
		drvdata->meta_locked = 0;
		break;
	case K90_USAGE_LIGHT_OFF:
	case K90_USAGE_LIGHT_DIM:
	case K90_USAGE_LIGHT_MEDIUM:
	case K90_USAGE_LIGHT_BRIGHT:
		drvdata->brightness = (usage->hid & HID_USAGE) - K90_USAGE_LIGHT;
		break;
	default:
		break;
	}

	return 0;
}

static int k90_input_mapping(struct hid_device *dev, struct hid_input *input,
		struct hid_field *field, struct hid_usage *usage,
		unsigned long **bit, int *max)
{
	int gkey;
	if ((gkey = k90_usage_to_gkey(usage->hid & HID_USAGE))) {
		hid_map_usage_clear(input, usage, bit, max, EV_KEY, k90_gkey_map[gkey-1]);
		return 1;
	}
	if ((usage->hid & HID_USAGE) >= K90_USAGE_SPECIAL_MIN &&
			(usage->hid & HID_USAGE) <= K90_USAGE_SPECIAL_MAX) {
		return -1;
	}
	return 0;
}

static const struct hid_device_id k90_devices[] = {
	{ HID_USB_DEVICE(USB_VENDOR_ID_CORSAIR, USB_DEVICE_ID_CORSAIR_K90) },
	{ }
};
MODULE_DEVICE_TABLE(hid, k90_devices);

static struct hid_driver k90_driver = {
	.name = "k90",
	.id_table = k90_devices,
	.probe = k90_probe,
	.event = k90_event,
	.remove = k90_remove,
	.input_mapping = k90_input_mapping,
};

static int k90_init(void)
{
	int ret;

	if (0 != (ret = hid_register_driver(&k90_driver))) {
		return ret;
	}

	return 0;
}

static void k90_exit(void)
{
	hid_unregister_driver(&k90_driver);
}

module_init(k90_init);
module_exit(k90_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Clément Vuchener");
MODULE_DESCRIPTION("HID driver for Corsair Vengeance K90 Keyboard");