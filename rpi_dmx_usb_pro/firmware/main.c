/**
 * @file main.c
 *
 */
/* Copyright (C) 2015 by Arjan van Vught <pm @ http://www.raspberrypi.org/forum/>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

#include "bcm2835.h"
#include "bcm2835_wdog.h"
#include "sys_time.h"
#include "usb.h"
#include "hardware.h"
#include "dmx.h"
#include "widget_params.h"
#include "rdm_device_info.h"
#include "widget.h"
#include "irq_led.h"

// poll table
extern void widget_received_rdm_packet(void);
extern void widget_receive_data_from_host(void);
extern void widget_ouput_dmx(void);
extern void widget_rdm_timeout(void);
extern void widget_sniffer_rdm(void);
extern void widget_sniffer_dmx(void);
// events table
extern void widget_received_dmx_packet(void);
extern void widget_received_dmx_change_of_state_packet(void);
extern void monitor_update(void);

void task_led(void) {
	static unsigned char led_counter = 0;
	hardware_led_set(led_counter++ & 0x01);
}

struct _poll
{
	void (*f)(void);
} const poll_table[] = {
		{ widget_received_rdm_packet },
		{ widget_ouput_dmx },
		{ widget_receive_data_from_host },
		{ widget_rdm_timeout },
		{ widget_sniffer_rdm },
		{ widget_sniffer_dmx }
		};

struct _event
{
	uint64_t period;
	void (*f)(void);
}const events[] = {
		{ 800000, widget_received_dmx_packet },
		{ 500000, widget_received_dmx_change_of_state_packet },
		{1000000, monitor_update },
		{ 500000, task_led}
};

uint64_t events_elapsed_time[sizeof(events) / sizeof(events[0])];

/**
 *
 */
static void events_init() {
	int i;
	uint64_t st_read_now = bcm2835_st_read();
	for (i = 0; i < (sizeof(events) / sizeof(events[0])); i++) {
		events_elapsed_time[i] += st_read_now;
	}
}

/**
 *
 */
static inline void events_check() {
	int i;
	uint64_t st_read_now = bcm2835_st_read();
	for (i = 0; i < (sizeof(events) / sizeof(events[0])); i++) {
		if (st_read_now > events_elapsed_time[i] + events[i].period) {
			events[i].f();
			events_elapsed_time[i] += events[i].period;
			//watchdog_feed();
		}
	}
}

int notmain(void)
{
	hardware_init();
	usb_init();
	dmx_init();
	widget_params_init();
	rdm_device_info_init();

	printf("Compiled on %s at %s ", __DATE__, __TIME__);
	const uint8_t *device_sn = rdm_device_info_get_sn();
	printf("[%d] S/N : %.2X%.2X%.2X%.2X\n", widget_mode_get(), device_sn[3],device_sn[2], device_sn[1], device_sn[0]);

	//watchdog_init();

	events_init();

	for (;;)
	{
		//watchdog_feed();
		int i = 0;
		for (i = 0; i < sizeof(poll_table) / sizeof(poll_table[0]); i++)
		{
			poll_table[i].f();
		}

		events_check();
	}

	return 0;
}
