#include<libusb-1.0/libusb.h>
#include<assert.h>
#include<stdio.h>

char buf[1024];

void print_info(libusb_device_handle *handle, char *format, int index)
{
	int len = libusb_get_string_descriptor_ascii(handle, index, buf, 1024);
	buf[len] = '\0';
	printf(format, buf);
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
		// Close all the stuff
		libusb_close(handle);
		return 1;
	}

	print_info(handle, "Manufacturer:  %s\n", desc.iManufacturer);
	print_info(handle, "Product:       %s\n", desc.iProduct);
	print_info(handle, "Serial number: %s\n", desc.iSerialNumber);

	struct libusb_config_descriptor *cdesc;
	retcode = libusb_get_active_config_descriptor(found, &cdesc);
	if (retcode) {
		printf("Could not retrieve active config descriptor (%d): %s\n", retcode, libusb_strerror(retcode));
		libusb_close(handle);
		return 1;
	}

	const struct libusb_endpoint_descriptor *to_stream = NULL, *from_stream = NULL;

	const struct libusb_interface *usbif;
	const struct libusb_interface_descriptor *idesc;
	const struct libusb_endpoint_descriptor *edesc;

	for (int i = 0; i < cdesc->bNumInterfaces; i++) {
		usbif = &cdesc->interface[i];
		int nalts = usbif->num_altsetting;
		for (int j = 0; j < nalts; j++) {
			idesc = &usbif->altsetting[j];
			for (int k = 0; k < idesc->bNumEndpoints; k++) {
				edesc = &idesc->endpoint[k];

				printf("endpoint descriptor(%d,%d,%d): ", i, j, k);
				printf("\n\tsize of endpoint descriptor : %d",edesc->bLength);
				printf("\n\ttype of descriptor : %d",edesc->bDescriptorType);
				printf("\n\tendpoint address : 0x0%x",edesc->bEndpointAddress);
				printf("\n\tmaximum packet size: %x",edesc->wMaxPacketSize);
				printf("\n\tattributes applied to endpoint: %d",edesc->bmAttributes);
				printf("\n\tinterval for polling for data tranfer : %d\n",edesc->bInterval);

				if (edesc->bEndpointAddress & LIBUSB_ENDPOINT_IN)
					from_stream = edesc;
				else
					to_stream = edesc;
			}
		}
	}

	if (to_stream == NULL || from_stream == NULL) {
		printf("Two endpoints expected: one in, one out.\n");
		libusb_close(handle);
		return 1;
	}

	// printf("Extra info 1: \"%s\", 2: \"%s\"\n", to_stream->extra, from_stream->extra);
	// printf("attrs: %#04x, %#04x\n", to_stream->bmAttributes, from_stream->bmAttributes);

	int written = 0;
	unsigned int timeout = 1000; // 1000 ms
	retcode = libusb_interrupt_transfer(handle, from_stream->bEndpointAddress, buf, 1024, &written, timeout);

	if (retcode) {
		printf("libusb_interrupt_transfer returned %d: %s\n", retcode, libusb_strerror(retcode));
		// libusb_close(handle);
		// return 1;
	}

	printf("%d bytes received:\n", written);
	buf[written] = '\0';
	printf("%s\n", buf);

	// Close all the stuff
	libusb_free_config_descriptor(cdesc);
	libusb_close(handle);
	return 0;
}
