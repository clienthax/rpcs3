#pragma once

#include <mutex>
#include <queue>
#include <libusb.h>

// Usb descriptors
enum : u8
{
	USB_DESCRIPTOR_DEVICE       = 0x01,
	USB_DESCRIPTOR_CONFIG       = 0x02,
	USB_DESCRIPTOR_STRING       = 0x03,
	USB_DESCRIPTOR_INTERFACE    = 0x04,
	USB_DESCRIPTOR_ENDPOINT     = 0x05,
	USB_DESCRIPTOR_HID          = 0x21,
	USB_DESCRIPTOR_ACI          = 0x24,
	USB_DESCRIPTOR_ENDPOINT_ASI = 0x25,
};

struct UsbDeviceDescriptor
{
	le_t<u16,1> bcdUSB;
	u8 bDeviceClass;
	u8 bDeviceSubClass;
	u8 bDeviceProtocol;
	u8 bMaxPacketSize0;
	le_t<u16,1> idVendor;
	le_t<u16,1> idProduct;
	le_t<u16,1> bcdDevice;
	u8 iManufacturer;
	u8 iProduct;
	u8 iSerialNumber;
	u8 bNumConfigurations;
};

struct UsbDeviceConfiguration
{
	le_t<u16,1> wTotalLength;
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;
};

struct UsbDeviceInterface
{
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bNumEndpoints;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
	u8 iInterface;
};

struct UsbDeviceEndpoint
{
	u8 bEndpointAddress;
	u8 bmAttributes;
	le_t<u16,1> wMaxPacketSize;
	u8 bInterval;
};

struct UsbDeviceEndpointAudio
{
	u8 bEndpointAddress;
	u8 bmAttributes;
	le_t<u16,1> wMaxPacketSize;
	u8 bInterval;
	u8 bRefresh;
	u8 bSynchAddress;
};

struct UsbDeviceHID
{
	le_t<u16,1> bcdHID;
	u8 bCountryCode;
	u8 bNumDescriptors;
	u8 bDescriptorType;
	le_t<u16,1> wDescriptorLength;
};

struct UsbDeviceACInterfaceHeader
{
	u8 bDescriptorSubtype;
	le_t<u16,1> bcdADC;
	le_t<u16,1> wTotalLength;
	u8 bInCollection;
	u8 baInterfaceNr;
};

struct UsbDeviceACInputTerminal
{
	u8 bDescriptorSubtype;
	u8 bTerminalID;
	le_t<u16,1> wTerminalType;
	u8 bAssocTerminal;
	u8 bNrChannels;
	le_t<u16,1> wChannelConfig;
	u8 iChannelNames;
	u8 iTerminal;
};

struct UsbDeviceACOutputTerminal
{
	u8 bDescriptorSubtype;
	u8 bTerminalID;
	le_t<u16,1> wTerminalType;
	u8 bAssocTerminal;
	u8 bSourceID;
	u8 iTerminal;
};

struct UsbDeviceACFeatureUnit
{
	u8 bDescriptorSubtype;
	u8 bUnitID;
	u8 bSourceID;
	u8 bControlSize;
	u8 Channel0;
	u8 Channel1;
	u8 Channel2;
	u8 iFeature;
};

struct UsbDeviceASInterface
{
	u8 bDescriptorSubtype;
	u8 bTerminalLink;
	u8 bDelay;
	u8 wFormatTag;
};

struct UsbDeviceASFormatType1
{
	u8 bDescriptorSubtype;
	u8 bFormatType;
	u8 bNrChannels;
	u8 bSubframeSize;
	u8 bBitResolution;
	u8 bSamFreqType;
	le_t<u16,1> tSamFreq1;
	u8 tSamFreq1High;
	le_t<u16,1> tSamFreq2;
	u8 tSamFreq2High;
	le_t<u16,1> tSamFreq3;
	u8 tSamFreq3High;
	le_t<u16,1> tSamFreq4;
	u8 tSamFreq4High;
	le_t<u16,1> tSamFreq5;
	u8 tSamFreq5High;
};

struct UsbDeviceASIsochronousDataEndpoint
{
	u8 bDescriptorSubtype;
	u8 bmAttributes;
	u8 bLockDelayUnits;
	le_t<u16,1> wLockDelay;
};

// PS3 internal codes
enum PS3StandardUsbErrors : u32
{
	HC_CC_NOERR = 0x00,
	EHCI_CC_MISSMF = 0x10,
	EHCI_CC_XACT = 0x20,
	EHCI_CC_BABBLE = 0x30,
	EHCI_CC_DATABUF = 0x40,
	EHCI_CC_HALTED = 0x50,
};

enum PS3IsochronousUsbErrors : u8
{
	USBD_HC_CC_NOERR = 0x00,
	USBD_HC_CC_MISSMF = 0x01,
	USBD_HC_CC_XACT = 0x02,
	USBD_HC_CC_BABBLE = 0x04,
	USBD_HC_CC_DATABUF = 0x08,
};

enum SysUsbdEvents : u32
{
	SYS_USBD_ATTACH            = 0x01,
	SYS_USBD_DETACH            = 0x02,
	SYS_USBD_TRANSFER_COMPLETE = 0x03,
	SYS_USBD_TERMINATE         = 0x04,
};

// PS3 internal structures
struct UsbInternalDevice
{
	u8 device_high; // System flag maybe (used in generating actual device number)
	u8 device_low;  // Just a number identifying the device (used in generating actual device number)
	u8 unk3;        // ? Seems to always be 2?
	u8 unk4;        // ?
};

struct UsbDeviceRequest
{
	u8 bmRequestType;
	u8 bRequest;
	be_t<u16> wValue;
	be_t<u16> wIndex;
	be_t<u16> wLength;
};

struct UsbDeviceIsoRequest
{
	vm::ptr<void> buf;
	be_t<u32> start_frame;
	be_t<u32> num_packets;
	be_t<u16> packets[8];
};

// Usb descriptor helper
struct UsbDescriptorNode
{
	u8 bLength;
	u8 bDescriptorType;

	union
	{
		UsbDeviceDescriptor _device;
		UsbDeviceConfiguration _configuration;
		UsbDeviceInterface _interface;
		UsbDeviceEndpoint _endpoint;
		UsbDeviceHID _hid;
		u8 data[0xFF];
	};

	std::vector<UsbDescriptorNode> subnodes;

	UsbDescriptorNode() {};
	template<typename T>
	UsbDescriptorNode(u8 _bDescriptorType, const T& _data) : bLength(sizeof(T) + 2), bDescriptorType(_bDescriptorType) { memcpy(data, &_data, sizeof(T)); }
	UsbDescriptorNode(u8 _bLength, u8 _bDescriptorType, u8* _data) : bLength(_bLength), bDescriptorType(_bDescriptorType) { memcpy(data, _data, _bLength-2); }

	UsbDescriptorNode& add_node(const UsbDescriptorNode& newnode) { subnodes.push_back(newnode); return subnodes.back(); }
	u32 get_size() const { u32 nodesize = bLength; for (const auto& node : subnodes) { nodesize += node.get_size(); } return nodesize; }
	void write_data(u8*& ptr) { ptr[0] = bLength; ptr[1] = bDescriptorType; memcpy(ptr+2, data, bLength-2); ptr += bLength;  for (auto& node : subnodes) { node.write_data(ptr); } }
};

// HLE usbd

struct UsbTransfer
{
	s32 result = 0;
	u32 count = 0;
	UsbDeviceIsoRequest iso_request;

	std::vector<u8> setup_buf;
	libusb_transfer *transfer = nullptr;
	bool busy = false;
};

class usb_device
{
public:
	virtual bool open_device() = 0;

	virtual void read_descriptors() {}

	virtual bool set_configuration(u8 cfg_num) = 0;
	virtual bool set_interface(u8 int_num) = 0;

	virtual void control_transfer(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u32 buf_size, u8* buf, UsbTransfer& transfer, u32 transfer_id) = 0;
	virtual void interrupt_transfer(u32 buf_size, u8 *buf, UsbTransfer& transfer, u32 transfer_id) = 0;
	virtual void isochronous_transfer(UsbTransfer& transfer, u32 transfer_id) = 0;

	u32 assigned_number = 0;
	u32 total_size = 0;

public:
	UsbDescriptorNode device;

protected:
	u32 current_config = 0;
	u32 current_interface = 0;

};

void LIBUSB_CALL callback_transfer(struct libusb_transfer *transfer);

class usb_device_passthrough : public usb_device
{
public:
	usb_device_passthrough(libusb_device *_device, libusb_device_descriptor& desc);
	~usb_device_passthrough();

	bool open_device() override;
	void read_descriptors() override;
	bool set_configuration(u8 cfg_num) override;
	bool set_interface(u8 int_num) override;
	void control_transfer(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u32 buf_size, u8* buf, UsbTransfer& transfer, u32 transfer_id) override;
	void interrupt_transfer(u32 buf_size, u8 *buf, UsbTransfer& transfer, u32 transfer_id) override;
	void isochronous_transfer(UsbTransfer& transfer, u32 transfer_id) override;

protected:
	libusb_device *lusb_device = nullptr;
	libusb_device_handle *lusb_handle = nullptr;
};

class usb_device_emulated : public usb_device
{
public:
	usb_device_emulated(const UsbDeviceDescriptor& _device);

	bool open_device() override;
	bool set_configuration(u8 cfg_num) override;
	bool set_interface(u8 int_num) override;
	void control_transfer(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u32 buf_size, u8* buf, UsbTransfer& transfer, u32 transfer_id) override;
	void interrupt_transfer(u32 buf_size, u8 *buf, UsbTransfer& transfer, u32 transfer_id) override;
	void isochronous_transfer(UsbTransfer& transfer, u32 transfer_id) override;

	// Emulated specific functions
	void add_string(char *str);
	s32 get_descriptor(u8 type, u8 index, u8 *ptr, u32 max_size);

protected:
	std::vector<std::string> strings;
};

struct UsbLdd
{
	std::string name;
	u16 id_vendor;
	u16 id_product_min;
	u16 id_product_max;
};

struct UsbPipe
{
	std::shared_ptr<usb_device> device;
	u32 endpoint;
};

class usb_handler_thread
{
public:
	usb_handler_thread();
	~usb_handler_thread();

	void operator()();

	void transfer_complete(struct libusb_transfer *transfer);

	u32 add_ldd(vm::ptr<char> s_product, u16 slen_product, u16 id_vendor, u16 id_product_min, u16 id_product_max);
	void check_devices_vs_ldds();

	u32 open_pipe(u32 device_handle, u32 endpoint);
	bool close_pipe(u32 pipe_number);

	// Map of devices actively handled by the ps3(device_id, device)
	std::map<u32, std::pair<UsbInternalDevice, std::shared_ptr<usb_device>>> handled_devices;
	// Queue of pending usbd events
	std::queue <std::tuple<u32, u32, u32>> usbd_events;
	// List of pipes
	std::map<u32, UsbPipe> open_pipes;
	// List of device drivers
	std::vector<UsbLdd> ldds;
	// Transfers infos
	std::array<UsbTransfer, 0x44> transfers;
	// Counters
	u8 dev_counter = 1;
	u32 pipe_counter = 0x10;
	u32 transfer_counter = 0;

private:
	// List of devices "connected" to the ps3
	std::vector<std::shared_ptr<usb_device>> usb_devices;

	libusb_context *ctx = nullptr;
	u32 open_error_counter = 0;
};

s32 sys_usbd_initialize(vm::ptr<u32> handle);
s32 sys_usbd_finalize(ppu_thread& ppu, u32 handle);
s32 sys_usbd_get_device_list(u32 handle, vm::ptr<UsbInternalDevice> device_list, u32 max_devices);
s32 sys_usbd_get_descriptor_size(u32 handle, u32 device_handle);
s32 sys_usbd_get_descriptor(u32 handle, u32 device_handle, vm::ptr<void> descriptor, u32 desc_size);
s32 sys_usbd_register_ldd(u32 handle, vm::ptr<char> s_product, u16 slen_product);
s32 sys_usbd_unregister_ldd();
s32 sys_usbd_open_pipe(u32 handle, u32 device_handle, u32 endpoint);
s32 sys_usbd_open_default_pipe(u32 handle, u32 device_handle);
s32 sys_usbd_close_pipe(u32 handle, u32 pipe_handle);
s32 sys_usbd_receive_event(ppu_thread& ppu, u32 handle, vm::ptr<u64> arg1, vm::ptr<u64> arg2, vm::ptr<u64> arg3);
s32 sys_usbd_detect_event();
s32 sys_usbd_attach(u32 handle);
s32 sys_usbd_transfer_data(u32 handle, u32 id_pipe, vm::ptr<void> buf_receive, u32 buf_size, vm::ptr<UsbDeviceRequest> request, u32 type_transfer);
s32 sys_usbd_isochronous_transfer_data(u32 handle, u32 id_pipe, vm::ptr<UsbDeviceIsoRequest> iso_request);
s32 sys_usbd_get_transfer_status(u32 handle, u32 id_transfer, u32 unk1, vm::ptr<u32> result, vm::ptr<u32> count);
s32 sys_usbd_get_isochronous_transfer_status(u32 handle, u32 id_transfer, u32 unk1, vm::ptr<UsbDeviceIsoRequest> request, vm::ptr<u32> result);
s32 sys_usbd_get_device_location();
s32 sys_usbd_send_event();
s32 sys_usbd_event_port_send();
s32 sys_usbd_allocate_memory();
s32 sys_usbd_free_memory();
s32 sys_usbd_get_device_speed();
s32 sys_usbd_register_extra_ldd(u32 handle, vm::ptr<char> s_product, u16 slen_product, u16 id_vendor, u16 id_product_min, u16 id_product_max);
