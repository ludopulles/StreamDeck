#include<libusb-1.0/libusb.h>
#include<assert.h>
#include<stdio.h>

char buf[1024];

int main()
{
    libusb_context *context = NULL;
	libusb_device **list;
	libusb_device *found = NULL;
	struct libusb_device_descriptor *desc;

	// create context
	int result = libusb_init(&context); 
	if (result < 0) {
		fprintf(stderr, "libusb_init() failed with %d.\n", result);
		return 1;
	}

	// discover devices
	int cnt = libusb_get_device_list(context, &list);
	if (cnt < 0)
		return printf("Error code (%d): %s\n", cnt, libusb_strerror(cnt)), 0;

	for (int i = 0; i < cnt; i++) {
		libusb_device *device = list[i];

		int ret = libusb_get_device_descriptor(device, desc);
		if (ret) {
			printf("Could not retrieve descriptor (%d): %s\n", ret, libusb_strerror(ret));
			continue;
		}

		// Pick the first Elgato Stream Deck
		if (desc->idVendor == 0xfd9 && desc->idProduct == 0x0060) {
			found = device;
			break;
		}
	}

	if (!found) {
		printf("No Stream Deck found.\n");
		libusb_free_device_list(list, 1);
		return 0;
	}

	libusb_device_handle *handle;
	int err = libusb_open(found, &handle);
	if (err) {
		printf("Error code while opening (%d): %s\n", err, libusb_strerror(err));
		return 0;
	}

	int len = libusb_get_string_descriptor_ascii(handle, desc->iManufacturer, buf, 1024);
	buf[len] = '\0';
	printf("Manufacturer:  %s\n", buf);

	len = libusb_get_string_descriptor_ascii(handle, desc->iProduct, buf, 1024);
	buf[len] = '\0';
	printf("Product:       %s\n", buf);
	
	len = libusb_get_string_descriptor_ascii(handle, desc->iSerialNumber, buf, 1024);
	buf[len] = '\0';
	printf("Serial Number: %s\n", buf);
	

	len = libusb_control_transfer(handle,
		LIBUSB_REQUEST_TYPE_STANDARD,

			uint8_t  	bmRequestType,
			uint8_t  	bRequest,
			uint16_t  	wValue,
			uint16_t  	wIndex,
			unsigned char *  	data,
			uint16_t  	wLength,
			unsigned int  	timeout 
			) 		

	// Close all the stuff
	libusb_close(handle);
	libusb_free_device_list(list, 1);
	return 0;
}
