#include <CoreServices/CoreServices.h>
#include <CoreFoundation/CFString.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach_port.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFBundle.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBLib.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

typedef unsigned int UINT;

bool IoRegistryGetProperty(io_service_t io_service, CFStringRef cfPropertyName,
						   UInt32 &lProperty)
{
	bool bResult = false;
	
	// Get property from io-registry
	CFTypeRef cfTypeReference = IORegistryEntryCreateCFProperty(io_service, cfPropertyName, kCFAllocatorDefault, kNilOptions);
	
	// Convert property to UInt32
	if( cfTypeReference )
	{
		bResult = CFNumberGetValue( (CFNumberRef)cfTypeReference, kCFNumberSInt32Type, &lProperty);
		CFRelease( cfTypeReference );
	}
	
	return bResult;
}

bool IoRegistryGetProperty(io_service_t io_service, CFStringRef cfPropertyName, char *prop)
{
	bool bResult = false;
	
	// Get property from io-registry
	CFTypeRef cfTypeReference = IORegistryEntryCreateCFProperty(io_service, cfPropertyName,
																kCFAllocatorDefault, kNilOptions);
	if (!cfTypeReference)
		return false;
	
	CFStringGetCString((CFStringRef)cfTypeReference, prop, 1024, kCFStringEncodingUTF8);
	CFRelease(cfTypeReference);
	return true;
}

static UINT GetUsbLocation(io_service_t usb)
{
	UINT locationID = 0;
	IoRegistryGetProperty(usb, CFSTR(kUSBDevicePropertyLocationID), locationID);
	return locationID;
}

static io_service_t GetUsbService(UINT uLocationId)
{
	io_iterator_t deviceIterator = 0;
	io_service_t usbDevice = 0;
 
	CFMutableDictionaryRef dictRef = IOServiceMatching(kIOUSBDeviceClassName/*"IOUSBHostDevice"*/);
	IOServiceGetMatchingServices(kIOMasterPortDefault, dictRef, &deviceIterator);
 
	while (IOIteratorIsValid(deviceIterator)) {
		usbDevice = IOIteratorNext(deviceIterator);
		if (!usbDevice)
			break;
		if (GetUsbLocation(usbDevice) == uLocationId)
			break;
		IOObjectRelease(usbDevice);
		usbDevice = 0;
	}
	IOObjectRelease(deviceIterator);
	return usbDevice;
}


static void dump_usb()
{
	io_iterator_t deviceIterator = 0;
	CFMutableDictionaryRef dictRef = IOServiceMatching(kIOUSBDeviceClassName);
	IOServiceGetMatchingServices(kIOMasterPortDefault, dictRef, &deviceIterator);
 
	while (IOIteratorIsValid(deviceIterator)) {
		io_service_t usbDevice = IOIteratorNext(deviceIterator);
		if (!usbDevice)
			break;

		char sFriendlyName[1024];
		IoRegistryGetProperty( usbDevice, CFSTR("USB Product Name"), sFriendlyName);

		printf("0x%08x %s\n", GetUsbLocation(usbDevice), sFriendlyName);
		IOObjectRelease(usbDevice);
		usbDevice = 0;
	}
	IOObjectRelease(deviceIterator);
}


static IOReturn GetUSBDeviceInterface(io_service_t usbDevice, IOUSBDeviceInterface650 ***iface)
{
	IOReturn ret;
	SInt32 score = 0;
	IOCFPlugInInterface **plugInInterface = NULL;
	IOCFPlugInInterface **	interface;
	IUnknownVTbl **		iunknown;
	
	ret = IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID,
											 kIOCFPlugInInterfaceID, &plugInInterface, &score);

	if (ret != kIOReturnSuccess || !plugInInterface) {
		printf("Failed to create PluginInterface: 0x%x\n", ret);
		return ret;
	}

	(*plugInInterface)->QueryInterface(plugInInterface,
            CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID650),
            (LPVOID*)iface);
	
	(*plugInInterface)->Release(plugInInterface);
 
	return 0;
}

int main(int argc, char *argv[])
{
	kern_return_t r;
	IOUSBDeviceInterface650 **iface;

	if (argc < 2) {
		dump_usb();
		return 0;
	}

	char *endptr;
	UINT uLocationId = strtol(argv[1], &endptr, 0);
	printf("searching for location 0x%08x\n", uLocationId);
	io_service_t usbDevice = GetUsbService(uLocationId);
	if (0 == usbDevice) {
		printf("not found\n");
		return -1;
	}
	bool do_release = false;
	if (argc > 2 && 0 == strcmp(argv[2], "release"))
		do_release = true;

	//r = IOServiceAuthorize(usbDevice, kIOServiceInteractionAllowed);
	//assert(0 == r);
 
	r = GetUSBDeviceInterface(usbDevice, &iface);
	if (0 != r) {
		printf("Failed to get device interface: 0x%x\n", r);
		return -1;
	}
 
	IOObjectRelease(usbDevice);
 
	if (do_release) {
		printf("release device\n");
		r = (*iface)->USBDeviceReEnumerate(iface,
						kUSBReEnumerateReleaseDeviceMask);
	}
	else {
		printf("capture device\n");
		r = (*iface)->USBDeviceReEnumerate(iface,
						kUSBReEnumerateCaptureDeviceMask);
	}

	printf("reenumerate result 0x%x\n", r);
	return 0;
}
