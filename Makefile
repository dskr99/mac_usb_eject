all:
	g++ eject_v2.cpp -framework IOKit -framework CoreFoundation -o usb_eject