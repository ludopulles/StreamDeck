#include<assert.h>
#include<dirent.h>
#include<libusb-1.0/libusb.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/stat.h>

char buf[1024];

void print_info(libusb_device_handle *handle, char *format, int index)
{
	int len = libusb_get_string_descriptor_ascii(handle, index, buf, 1024);
	buf[len] = '\0';
	printf(format, buf);
}

int time_before(struct timespec *a, struct timespec *b)
{
	if (a->tv_sec != b->tv_sec)
		return a->tv_sec < b->tv_sec;
	return a->tv_nsec < b->tv_nsec;
}

char* last_modified_kattis()
{
	DIR *dir;
	struct dirent *ent;
	if ((dir = opendir("/home/ludop/code/kattis/")) == NULL) {
		// could not open directory
		return NULL;
	}

	// print all the files and directories within directory
	char fullname[256 + 64] = "/home/ludop/code/kattis/";
	struct stat fstat;

	char *last_edited = (char*) malloc(256 + 64);
	struct timespec last_ctime = { 0, 0 };
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_type != DT_REG || !strcmp("a.out", ent->d_name)) continue;
		strcpy(fullname + 24, ent->d_name);
		stat(fullname, &fstat);

		if (time_before(&last_ctime, &fstat.st_ctim)) {
			last_ctime = fstat.st_ctim;
			strcpy(last_edited, fullname);
		}
	}
	closedir (dir);
	return last_edited;
}

void button_pressed(int b)
{
	char *ans = last_modified_kattis();
	if (ans != NULL) {
		int anslen = strlen(ans);
		// printf("The winner is: %s (%d)\n", ans, anslen);
		char *cmd = (char*) malloc(anslen + 22);
		strcpy(cmd, "su ludop -c \"hattis ");
		strcpy(cmd + 20, ans);
		free(ans);
		cmd[anslen + 20] = '\"';
		cmd[anslen + 21] = '\0';
		printf("Command: %s\n", cmd);

		int retcode = system(cmd);
		// int retcode = 15;
		printf("Retcode: %d\n", retcode);

		free(cmd);
	}
}

void button_released(int b)
{

}

int main()
{
    libusb_context *context = NULL;
	libusb_device **list;

	// create context
	int retcode = libusb_init(&context);
	if (retcode < 0) {
		fprintf(stderr, "libusb_init() failed with %d.\n", retcode);
		return 1;
	}

	// discover devices
	int cnt = libusb_get_device_list(context, &list);
	if (cnt < 0)
		return printf("Error code (%d): %s\n", cnt, libusb_strerror(cnt)), 0;

	libusb_device *found = NULL;
	struct libusb_device_descriptor desc;
	for (int i = 0; i < cnt; i++) {
		retcode = libusb_get_device_descriptor(list[i], &desc);
		if (retcode) {
			printf("Could not retrieve descriptor (%d): %s\n", retcode, libusb_strerror(retcode));
			continue;
		}

		// Pick the first Elgato Stream Deck
		if (desc.idVendor == 0xfd9 && desc.idProduct == 0x0060) {
			found = list[i];
			break;
		}
	}

	libusb_free_device_list(list, 1);

	if (!found) {
		printf("No Stream Deck found.\n");
		return 255;
	}

	libusb_device_handle *handle;
	int err = libusb_open(found, &handle);
	if (err) {
		printf("Error code while opening (%d): %s\n", err, libusb_strerror(err));
		return 1;
	}
/*
	print_info(handle, "Manufacturer:  %s\n", desc.iManufacturer);
	print_info(handle, "Product:       %s\n", desc.iProduct);
	print_info(handle, "Serial number: %s\n", desc.iSerialNumber);
*/
	print_info(handle, "Connected to Elgato Stream Deck %s...\n", desc.iSerialNumber);

	struct libusb_config_descriptor *cdesc;
	retcode = libusb_get_active_config_descriptor(found, &cdesc);
	if (retcode) {
		printf("Could not retrieve active config descriptor (%d): %s\n", retcode, libusb_strerror(retcode));
		libusb_close(handle);
		return 1;
	}

	if (cdesc->bNumInterfaces != 1) {
		printf("Unexpected number of interfaces: %d\n", cdesc->bNumInterfaces);
		return 1;
	}
	const struct libusb_interface *usbif = &cdesc->interface[0];

	if (usbif->num_altsetting != 1) {
		printf("Unexpected number of altsettings: %d\n", usbif->num_altsetting);
		return 1;
	}
	const struct libusb_interface_descriptor *idesc = &usbif->altsetting[0];

	const struct libusb_endpoint_descriptor *edesc, *to_stream = NULL, *from_stream = NULL;
	for (int k = 0; k < idesc->bNumEndpoints; k++) {
		edesc = &idesc->endpoint[k];
/*
		printf("endpoint descriptor #%d: ", k);
		printf("\n\tsize of endpoint descriptor : %d",edesc->bLength);
		printf("\n\ttype of descriptor : %d",edesc->bDescriptorType);
		printf("\n\tendpoint address : 0x0%x",edesc->bEndpointAddress);
		printf("\n\tmaximum packet size: %x",edesc->wMaxPacketSize);
		printf("\n\tattributes applied to endpoint: %d",edesc->bmAttributes);
		printf("\n\tinterval for polling for data tranfer : %d\n",edesc->bInterval);
*/
		if (edesc->bEndpointAddress & LIBUSB_ENDPOINT_IN) from_stream = edesc;
		else to_stream = edesc;
	}

	if (to_stream == NULL || from_stream == NULL) {
		printf("Two endpoints expected: one in, one out.\n");
		libusb_close(handle);
		return 1;
	}

	retcode = libusb_set_auto_detach_kernel_driver(handle, 1);
	if (retcode) {
		printf("libusb_set_auto_detach_kernel_driver (%d): %s\n", retcode, libusb_strerror(retcode));
		return 1;
	}
	retcode = libusb_claim_interface(handle, 0);
	if (retcode) {
		printf("Could not claim interface 0 (%d): %s\n", retcode, libusb_strerror(retcode));
		return 1;
	}

	int written;
	unsigned int timeout = 0;

	int last_state = 0;

	while (1) {
		retcode = libusb_interrupt_transfer(handle, from_stream->bEndpointAddress, buf, 1024, &written, timeout);
		if (retcode) {
			printf("libusb_interrupt_transfer returned %d: %s\n", retcode, libusb_strerror(retcode));
			break;
		}

		int cur_state = 0;
		for (int i = 1; i <= 15; i++) {
			if (buf[i] != 0)
				cur_state |= 1 << i;
		}

		int pressed = cur_state & ~last_state;
		int released = ~cur_state & last_state;

		if (pressed) {
			for (int i = 1, j = 2; i <= 15; i++, j <<= 1)
				if (cur_state & ~last_state & j)
					button_pressed(i);

			printf("Buttons pressed: ");
			int first = 1;
			for (int i = 1, j = 2; i <= 15; i++, j <<= 1)
				if (cur_state & ~last_state & j) {
					if (first) first = 0;
					else printf(", ");
					printf("%d", i);
				}
			printf("\n");

		}

		if (released) {
			for (int i = 1, j = 2; i <= 15; i++, j <<= 1)
				if (~cur_state & last_state & j)
					button_released(i);

			printf("Buttons released: ");
			int first = 1;
			for (int i = 1, j = 2; i <= 15; i++, j <<= 1)
				if (~cur_state & last_state & j) {
					if (first) first = 0;
					else printf(", ");
					printf("%d", i);
				}
			printf("\n");
		}

		last_state = cur_state;
	}

	// Close all the stuff
	libusb_release_interface(handle, 0);
	libusb_free_config_descriptor(cdesc);
	libusb_close(handle);
	return 0;
}
