/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#define LOG_NDEBUG 0
#define LOG_TAG "lights"

#include <cutils/log.h>

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

enum{
	LED_OFF,
	SLOW_BLINKING,
	FAST_BLINKING,
	ALWAYS_ON,
};

#define CALL_NOTIFICATION   0x1
#define MAIL_NOTIFICATION   0x2

char const*const POWER_LED_FILE
        = "/sys/class/leds2/power";

char const*const MAIL_LED_FILE
        = "/sys/class/leds2/mail";

char const*const CALL_LED_FILE
        = "/sys/class/leds2/call";

char const*const BOTT_LED_FILE
        = "/sys/class/leds2/bottom";

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

static int write_int(char const* path, int value) {
	int fd;
	static int already_warned = 0;

	fd = open(path, O_WRONLY);
	if (fd >= 0) {
		char buffer[20];
		int bytes = sprintf(buffer, "%d\n", value);
		int amt = write(fd, buffer, bytes);
		close(fd);
		return amt == -1 ? -errno : 0;
	} else {
		if (already_warned == 0) {
			LOGE("write_int failed to open %s\n", path);
			already_warned = 1;
		}
		return -errno;
	}
}

static int is_lit(struct light_state_t const* state) {
    return state->color & 0x00ffffff;
}

#define IOCTL_SET_BL_LV           _IOW(0x11, 3, int)
static int set_light_backlight(struct light_device_t* dev,
		struct light_state_t const* state) {
	int fd=open("/dev/avr", O_RDWR);
	//From lights.h
	int color=state->color;
	unsigned char brightness = (state->color)&0xff;
	ioctl(fd, IOCTL_SET_BL_LV, brightness);
	close(fd);
	return 0;
}

static int set_light_battery(struct light_device_t* dev,
		struct light_state_t const* state) {
	int led_state = LED_OFF;

	if(state->color == 0xFFFF0000) {
		if(state->flashMode==LIGHT_FLASH_TIMED) {
			//Low and not charging
			//Fast blink
			led_state = FAST_BLINKING;
		} else {
			//Low and charging
			//Slow blink
			led_state = SLOW_BLINKING;
		}
	} else if(state->color==0xFF00FF00) {
		//Charging and full
		//Fixed
		led_state = ALWAYS_ON;
	} else if(state->color==0xFFFFFF00) {
		//Charging
		led_state = SLOW_BLINKING;
	} else {
		//Off
		led_state = LED_OFF;
	}

	pthread_mutex_lock(&g_lock);
	write_int(POWER_LED_FILE, led_state);
	pthread_mutex_unlock(&g_lock);

	LOGE("Light battery: %p\n", state->color);
	return 0;
}

static int set_light_notifications(struct light_device_t* dev,
		struct light_state_t const* state) {
	int notification_type = -1;
	int led_state = LED_OFF;

			if(state->color == 0xffffffff) {
				//Notification on
				//Slow blink
				LOGE("MAIL WRITE");
				//write_int(CALL_LED_FILE, 3);
				notification_type = MAIL_NOTIFICATION;
				led_state = SLOW_BLINKING;
			} 
			else if(state->color == 0x00) {
				//Notification off
				//Off
				LOGE("OFF");
				notification_type = MAIL_NOTIFICATION | CALL_NOTIFICATION;
				led_state = LED_OFF;
			} 
			else { //ANY COLOR 
				//Notification on
				//Slow blink
				//write_int(MAIL_LED_FILE, 0);
				LOGE("CALL WRITE");
				notification_type = CALL_NOTIFICATION;
				led_state = SLOW_BLINKING;
			}

			if ((CALL_NOTIFICATION & notification_type) == CALL_NOTIFICATION) {
				pthread_mutex_lock(&g_lock);
				write_int(CALL_LED_FILE, led_state);
				pthread_mutex_unlock(&g_lock);
			}

			if ((MAIL_NOTIFICATION & notification_type) == MAIL_NOTIFICATION) {
				pthread_mutex_lock(&g_lock);
				write_int(MAIL_LED_FILE, led_state);
				pthread_mutex_unlock(&g_lock);
			}

	LOGE("Notification led: %p(%d,%d,%d)\n", state->color, state->flashMode, state->flashOnMS, state->flashOffMS);
	return 0;
}

static int
set_light_buttons(struct light_device_t* dev,
        struct light_state_t const* state)
{
    return 0;
}


/** Close the lights device */
static int close_lights(struct light_device_t *dev) {
	if (dev) {
		free(dev);
	}
	return 0;
}


/******************************************************************************/

/**
 * module methods
 */

/** Open a new instance of a lights device using name */
static int open_lights(const struct hw_module_t* module, char const* name,
		struct hw_device_t** device) {
	int (*set_light)(struct light_device_t* dev,
			struct light_state_t const* state);

	if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
		set_light = set_light_backlight;
	} else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
		set_light = set_light_buttons;
	} else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
		set_light = set_light_battery;
	} else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
		set_light = set_light_notifications;
	} else {
		return -EINVAL;
	}

	pthread_mutex_init(&g_lock, NULL);

	struct light_device_t *dev = malloc(sizeof(struct light_device_t));
	memset(dev, 0, sizeof(*dev));

	dev->common.tag = HARDWARE_DEVICE_TAG;
	dev->common.version = 0;
	dev->common.module = (struct hw_module_t*)module;
	dev->common.close = (int (*)(struct hw_device_t*))close_lights;
	dev->set_light = set_light;

	*device = (struct hw_device_t*)dev;
	return 0;
}


static struct hw_module_methods_t lights_module_methods = {
	.open =	open_lights,
};

/*
 * The lights Module
 */
const struct hw_module_t HAL_MODULE_INFO_SYM = {
	.tag = HARDWARE_MODULE_TAG,
	.version_major = 1,
	.version_minor = 0,
	.id = LIGHTS_HARDWARE_MODULE_ID,
	.name = "Acer Liquid lights Module",
	.author = "ThePasto <thepasto@gmail.com>",
	.methods = &lights_module_methods,
};
