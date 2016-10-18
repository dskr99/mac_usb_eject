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

static UINT get_facetime()
{
	UINT rv = 0xffffffff;
	io_iterator_t deviceIterator = 0;
	CFMutableDictionaryRef dictRef = IOServiceMatching(kIOUSBDeviceClassName);
	IOServiceGetMatchingServices(kIOMasterPortDefault, dictRef, &deviceIterator);
 
	while (IOIteratorIsValid(deviceIterator)) {
		io_service_t usbDevice = IOIteratorNext(deviceIterator);
		if (!usbDevice)
			break;
		
		char sFriendlyName[1024];
		IoRegistryGetProperty( usbDevice, CFSTR("USB Product Name"), sFriendlyName);
		UINT location = GetUsbLocation(usbDevice);
		IOObjectRelease(usbDevice);
		if (NULL != strstr(sFriendlyName, "FaceTime")) {
			rv = location;
			break;
		}
	}
	IOObjectRelease(deviceIterator);
	return rv;
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

	printf("========= list of usb devices =========\n");
	dump_usb();
	printf("======= end of usb devices list =======\n\n");

	UINT uLocationId = get_facetime();
	if (uLocationId == 0xffffffff) {
		printf("No facetime camera found, exiting.\n");
		return -1;
	}
	printf("Facetime camera found at 0x%08x\n", uLocationId);
	if (geteuid() != 0) {
		printf("Error, not a root. Execute 'sudo %s'\n", argv[0]);
		return -1;
	}

	io_service_t usbDevice = GetUsbService(uLocationId);
	if (0 == usbDevice) {
		printf("Failed to open facetime camera device.\n");
		return -1;
	}

	r = GetUSBDeviceInterface(usbDevice, &iface);
	if (0 != r) {
		printf("Failed to get facetime camera device interface: 0x%x\n", r);
		return -1;
	}
 
	IOObjectRelease(usbDevice);
 
	printf("replugging the Facetime camera device..\n");
	r = (*iface)->USBDeviceReEnumerate(iface,
					kUSBReEnumerateReleaseDeviceMask);

	printf("replug result 0x%x\n", r);
	return 0;
}
