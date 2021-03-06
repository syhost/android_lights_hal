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


// #define LOG_NDEBUG 0
#define LOG_TAG "lights"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>

#include <sys/ioctl.h>
#include <sys/types.h>

#include <hardware/lights.h>

/******************************************************************************/

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static struct light_state_t g_notification;
static struct light_state_t g_battery;
static int g_backlight = 255;
static int g_buttons = 0;
static int g_attention = 0;

#define MAX_BRIGHTNESS 255

#define LP5523_LEDS			8 //9

#define EF59_LED1_GREEN     0
#define EF59_LED1_BLUE      1
#define EF59_LED2_GREEN     2
#define EF59_LED2_BLUE      3
#define EF59_MENU_KEY       4
#define EF59_BACK_KEY       5
#define EF59_LED1_RED       6
#define EF59_LED2_RED       7

char const*const MENU_LED_FILE = "/sys/class/leds/lp5523:channel4/brightness";
char const*const BACK_LED_FILE = "/sys/class/leds/lp5523:channel5/brightness";
char const*const RED_R_LED_FILE = "/sys/class/leds/lp5523:channel6/brightness";
char const*const GREEN_R_LED_FILE = "/sys/class/leds/lp5523:channel0/brightness";
char const*const BLUE_R_LED_FILE = "/sys/class/leds/lp5523:channel1/brightness";
char const*const RED_L_LED_FILE = "/sys/class/leds/lp5523:channel7/brightness";
char const*const GREEN_L_LED_FILE = "/sys/class/leds/lp5523:channel2/brightness";
char const*const BLUE_L_LED_FILE = "/sys/class/leds/lp5523:channel3/brightness";
char const*const LCD_FILE = "/sys/class/leds/lcd-backlight/brightness";

char const*const LED_WRITEON_FILE = "/dev/led_fops";

char const*const BAT_CAP_FILE = "/sys/class/power_supply/battery/capacity";

static int set_light_keyboard(struct light_device_t* dev, struct light_state_t const* state);
static int set_light_buttons(struct light_device_t* dev, struct light_state_t const* state);

/**
 * device methods
 */

void init_globals(void)
{
    // init the mutex
    pthread_mutex_init(&g_lock, NULL);
}

static int write_int(char const* path, int value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int write_str(char const* path, char *value)
{
    int fd;
    static int already_warned = 0;

    fd = open(path, O_RDWR);
    if (fd >= 0) {
        char buffer[PAGE_SIZE];
        int bytes = sprintf(buffer, "%s\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_str failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int read_int(char const *path)
{
    int fd;
    char buffer[2];

    fd = open(path, O_RDONLY);

    if (fd >= 0) {
        read(fd, buffer, 1);
    }
    close(fd);

    return atoi(buffer);
}

static int is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}

static int rgb_to_brightness(struct light_state_t const* state)
{
    int color = state->color & 0x00ffffff;
    int brightness = ((77*((color>>16) & 0x00ff)) + (150*((color>>8) & 0x00ff)) + (29*(color & 0x00ff))) >> 8;

	if (brightness > MAX_BRIGHTNESS)
		brightness = MAX_BRIGHTNESS;
    return brightness;
}

static int set_light_backlight(struct light_device_t* dev, struct light_state_t const* state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);
    
    pthread_mutex_lock(&g_lock);
    g_backlight = brightness;
    err = write_int(LCD_FILE, brightness);
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_light_keyboard(struct light_device_t* dev, struct light_state_t const* state)
{
    int err = 0;
    int on = is_lit(state);
    int brightness = rgb_to_brightness(state);
    pthread_mutex_lock(&g_lock);
    if(on)
	{
		write_int(MENU_LED_FILE, brightness);
		write_str(LED_WRITEON_FILE, "writeon5");
		write_int(BACK_LED_FILE, brightness);
		write_str(LED_WRITEON_FILE, "writeon6");
    }
    else
    {
    	write_str(LED_WRITEON_FILE, "writeoff5");
    	write_str(LED_WRITEON_FILE, "writeoff6");
    }
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_light_buttons(struct light_device_t* dev, struct light_state_t const* state)
{
    int err = 0;
    int on = is_lit(state);
    int brightness = rgb_to_brightness(state);
    pthread_mutex_lock(&g_lock);
    if(on)
	{
		write_int(MENU_LED_FILE, brightness);
		write_str(LED_WRITEON_FILE, "writeon5");
		write_int(BACK_LED_FILE, brightness);
		write_str(LED_WRITEON_FILE, "writeon6");
    }
    else
    {
    	write_str(LED_WRITEON_FILE, "writeoff5");
    	write_str(LED_WRITEON_FILE, "writeoff6");
    }
    pthread_mutex_unlock(&g_lock);
    return err;
}

static int set_speaker_light_locked(struct light_device_t* dev, struct light_state_t const* state)
{
    int alpha, red, green, blue;
    unsigned int colorRGB;
    int i;
    char buffer[PAGE_SIZE];
    int on = is_lit(state);

	if(on)
	{
		colorRGB = state->color;

		red = (colorRGB >> 16) & 0xFF;
		green = (colorRGB >> 8) & 0xFF;
		blue = colorRGB & 0xFF;
		ALOGD("set_speaker_light_locked R=%d,G=%d,B=%d\n", red, green, blue);

		write_int(RED_R_LED_FILE, red);
		write_int(RED_L_LED_FILE, red);
		write_int(GREEN_R_LED_FILE, green);
		write_int(GREEN_L_LED_FILE, green);
		write_int(BLUE_R_LED_FILE, blue);
		write_int(BLUE_L_LED_FILE, blue);
		for(i=0; i < LP5523_LEDS; i++)
		{
			if(i==EF59_MENU_KEY || i==EF59_BACK_KEY) continue;
			sprintf(buffer, "writeon%d\n", i+1);
			write_str(LED_WRITEON_FILE, buffer);
		}
	}
	else
	{
		for(i=0; i<LP5523_LEDS; i++)
		{
			if(i==EF59_MENU_KEY || i==EF59_BACK_KEY) continue;
			sprintf(buffer, "writeoff%d\n", i+1);
			write_str(LED_WRITEON_FILE, buffer);
		}
	}
    return 0;
}

static void handle_speaker_battery_locked(struct light_device_t* dev)
{
    if (!is_lit(&g_notification)) {
        set_speaker_light_locked(dev, &g_battery);
    } else {
        set_speaker_light_locked(dev, &g_notification);
    }
}

static int set_light_battery(struct light_device_t* dev, struct light_state_t const* state)
{
	int alpha, red, green, blue;
	unsigned int colorRGB;
	int on = is_lit(state);
	int bat_cap = read_int(BAT_CAP_FILE);
	
    pthread_mutex_lock(&g_lock);
    g_battery = *state;
    ALOGD("set_light_battery color=0x%08x", state->color);
	colorRGB = state->color;

	red = (colorRGB >> 16) & 0xFF;
	green = (colorRGB >> 8) & 0xFF;
	blue = colorRGB & 0xFF;
	ALOGD("set_speaker_light_locked R=%d,G=%d,B=%d\n", red, green, blue);
    //handle_speaker_battery_locked(dev);
    if(on)
	{
		if(bat_cap > 95)
		{
			write_str(LED_WRITEON_FILE, "reset");
			
			write_int(GREEN_R_LED_FILE, green);
			write_int(GREEN_L_LED_FILE, green);
			write_str(LED_WRITEON_FILE, "writeon1");
			write_str(LED_WRITEON_FILE, "writeon3");
		}
		else
		{
			write_str(LED_WRITEON_FILE, "writeoff1");
			write_str(LED_WRITEON_FILE, "writeoff3");
			write_str(LED_WRITEON_FILE, "red_dim");
		}
	}
	else
	{
		write_str(LED_WRITEON_FILE, "reset");
	}
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_notifications(struct light_device_t* dev, struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    g_notification = *state;
    ALOGD("set_light_notifications color=0x%08x", state->color);
    handle_speaker_battery_locked(dev);
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static int set_light_attention(struct light_device_t* dev, struct light_state_t const* state)
{
    pthread_mutex_lock(&g_lock);
    ALOGD("set_light_attention color=0x%08x", state->color);
    if (state->flashMode == LIGHT_FLASH_HARDWARE) {
        g_attention = state->flashOnMS;
    } else if (state->flashMode == LIGHT_FLASH_NONE) {
        g_attention = 0;
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}


/** Close the lights device */
static int close_lights(struct light_device_t *dev)
{
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
static int open_lights(const struct hw_module_t* module, char const* name, struct hw_device_t** device)
{
    int (*set_light)(struct light_device_t* dev,
            struct light_state_t const* state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name)) {
        set_light = set_light_backlight;
    }
    else if (0 == strcmp(LIGHT_ID_KEYBOARD, name)) {
        set_light = set_light_keyboard;
    }
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name)) {
        set_light = set_light_buttons;
    }
    else if (0 == strcmp(LIGHT_ID_BATTERY, name)) {
        set_light = set_light_battery;
    }
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
        set_light = set_light_notifications;
    }
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name)) {
        set_light = set_light_attention;
    }
    else {
        return -EINVAL;
    }

    pthread_once(&g_init, init_globals);

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
    .open =  open_lights,
};

/*
 * The lights Module
 */
struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "Pantech lights Module",
    .author = "soyudesign@gmail.com",
    .methods = &lights_module_methods,
};
