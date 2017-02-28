#include <linux/input.h>
#include <linux/uinput.h>

#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <hidapi/hidapi.h>

#define die(str, args...) do { \
        perror(str); \
        exit(EXIT_FAILURE); \
    } while(0)

void send_keycode(int fd, int keycode)
{
    struct input_event event;
    int ecode = 0;

    switch (keycode) {
    case 0x30:
	ecode = KEY_0;
	break;
    case 0x31:
	ecode = KEY_1;
	break;
    case 0x32:
	ecode = KEY_2;
	break;
    case 0x33:
	ecode = KEY_3;
	break;
    case 0x34:
	ecode = KEY_4;
	break;
    case 0x35:
	ecode = KEY_5;
	break;
    case 0x36:
	ecode = KEY_6;
	break;
    case 0x37:
	ecode = KEY_7;
	break;
    case 0x38:
	ecode = KEY_8;
	break;
    case 0x39:
	ecode = KEY_9;
	break;
    case 0x61:
	ecode = KEY_A;
	break;
    case 0x62:
	ecode = KEY_B;
	break;
    case 0x63:
	ecode = KEY_C;
	break;
    case 0x64:
	ecode = KEY_D;
	break;
    case 0x65:
	ecode = KEY_E;
	break;
    case 0x66:
	ecode = KEY_F;
	break;
    case 0x1c:
	ecode = KEY_ENTER;
	break;
    default:
	printf("unknown code: %02x\n", keycode);
	return;
    }
#ifdef DEBUG
    printf("%i = %i\n", keycode, ecode);
#endif

// press key
    memset(&event, 0, sizeof(struct input_event));
    event.type = EV_KEY;
    event.code = ecode;
    event.value = 1;
    if (write(fd, &event, sizeof(struct input_event)) < 0)
	die("error: write event");
// sync
    memset(&event, 0, sizeof(struct input_event));
    event.type = EV_SYN;
    event.code = 0;
    event.value = 0;
    if (write(fd, &event, sizeof(struct input_event)) < 0)
	die("error: write event");
// release key
    memset(&event, 0, sizeof(struct input_event));
    event.type = EV_KEY;
    event.code = ecode;
    event.value = 0;
    if (write(fd, &event, sizeof(struct input_event)) < 0)
	die("error: write event");
// sync
    memset(&event, 0, sizeof(struct input_event));
    event.type = EV_SYN;
    event.code = 0;
    event.value = 0;
    if (write(fd, &event, sizeof(struct input_event)) < 0)
	die("error: write event");
}

int main(int argc, char *argv[])
{
    int res, i, fd;
    unsigned char command[64] =
	{ 0xae, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x06 };
    unsigned char buf[64] =
	{ 0xae, 0x00, 0x04, 0x00, 0x02, 0x00, 0x00, 0x07 };
    hid_device *handle;
    struct hid_device_info *devs, *cur_dev;
    struct uinput_user_dev uidev;
    char hex_tmp[64];

    sem_t *mysem = sem_open("rfidtestsem", O_CREAT|O_EXCL);
    if ( mysem == NULL )
    {
        die("error: already running\n");
    }

// create uinput device
    fd = open("/dev/uinput", O_WRONLY);
    if (fd < 0)
	die("uinput open:");
// enable uinput key output
    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
	die("error: UI_SET_EVBIT");
// enable way too many keys...
    for (i = KEY_RESERVED; i < KEY_UNKNOWN; i++)
	if (ioctl(fd, UI_SET_KEYBIT, i) < 0)
	    die("error: UI_SET_KEYBIT");
// create uinput device
    memset(&uidev, 0, sizeof(struct uinput_user_dev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "rfidtest");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor = 0x1667;
    uidev.id.product = 0x0026;
    uidev.id.version = 1;
    if (write(fd, &uidev, sizeof(uidev)) < 0)
	die("error: write uidev");
    if (ioctl(fd, UI_DEV_CREATE) < 0)
	die("error: ioctl UI_DEV_CREATE");
// search rfid reader
    devs = hid_enumerate(0x0, 0x0);
    cur_dev = devs;
    while (cur_dev) {
#ifdef DEBUG
	printf
	    ("Device Found\n  type: %04hx %04hx\n  path: %s\n  serial_number: %ls",
	     cur_dev->vendor_id, cur_dev->product_id, cur_dev->path,
	     cur_dev->serial_number);
	printf("\n");
	printf("  Manufacturer: %ls\n", cur_dev->manufacturer_string);
	printf("  Product:      %ls\n", cur_dev->product_string);
	printf("  Release:      %hx\n", cur_dev->release_number);
	printf("  Interface:    %d\n", cur_dev->interface_number);
	printf("\n");
#endif
// in case found rfid reader
	if (cur_dev->vendor_id == 0x1667 && cur_dev->product_id == 0x0026) {
	    handle = hid_open_path(cur_dev->path);
	    while (1) {
// read rfid code
		hid_write(handle, command, 64);
		hid_read(handle, buf, 64);
		res = hid_read(handle, buf, 64);
		if (res < 0)
		    die("Unable to read rfid code\n");
// reader outputs the code as four pairs of hex
// convert to one hex per byte
		for (i = 5; i < (5 + 8); i++)
		    sprintf(&hex_tmp[(i - 5) * 2], "%02x", buf[i]);
#ifdef DEBUG
		for (i = 0; i < res; i++)
		    printf("%02x,", buf[i]);
		printf("\n");
		for (i = 0; i < 8; i++)
		    printf("%i,", hex_tmp[i]);
		printf("\n");
#endif
// send keystrokes
		for (i = 0; i < 8; i++)
		    send_keycode(fd, hex_tmp[i]);
// press enter
		send_keycode(fd, KEY_ENTER);
/*
	    command[5] = 0x01;
	    command[6] = 0x00;
	    hid_write(handle, command, 64);
	    res = hid_read(handle, buf, 64);
	    if (res < 0) {
		printf("Unable to read()\n");
		break;
	    }
*/
//      sleep(1);
	    }			// while 1
	}			// if cur_dev
// loop all HID devices
	cur_dev = cur_dev->next;
    }				// while

// destroy uinput device
    if (ioctl(fd, UI_DEV_DESTROY) < 0)
	die("error: ioctl");

    close(fd);
    hid_free_enumeration(devs);
    return 0;
}
