#include "stdafx.h"
#include <queue>
#include "Emu/Memory/vm.h"
#include "Emu/System.h"

#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/ErrorCodes.h"
#include "sys_usbd.h"
#include "sys_ppu_thread.h"
#include "Emu/Cell/lv2/sys_event.h"
#include "Emu/Cell/lv2/sys_process.h"
#include "Emu/Cell/lv2/sys_timer.h"

#include <libusb.h>

LOG_CHANNEL(sys_usbd);

//////////////////////////////////////////////////////////////////
// PASSTHROUGH DEVICE ////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
usb_device_passthrough::usb_device_passthrough(libusb_device *_device, libusb_device_descriptor& desc) : lusb_device(_device)
{
	device = UsbDescriptorNode(USB_DESCRIPTOR_DEVICE, UsbDeviceDescriptor
		{
			desc.bcdUSB,
			desc.bDeviceClass,
			desc.bDeviceSubClass,
			desc.bDeviceProtocol,
			desc.bMaxPacketSize0,
			desc.idVendor,
			desc.idProduct,
			desc.bcdDevice,
			desc.iManufacturer,
			desc.iProduct,
			desc.iSerialNumber,
			desc.bNumConfigurations
		});
}

usb_device_passthrough::~usb_device_passthrough()
{
	if (lusb_handle) libusb_close(lusb_handle);
	if (lusb_device) libusb_unref_device(lusb_device);
}

bool usb_device_passthrough::open_device()
{
	return libusb_open(lusb_device, &lusb_handle) == LIBUSB_SUCCESS;
}

void usb_device_passthrough::read_descriptors()
{
	// Directly getting configuration descriptors from the device instead of going through libusb parsing functions as they're not needed
	for (u8 index = 0; index < device._device.bNumConfigurations; index++)
	{
		u8 buf[1000];
		int ssize = libusb_control_transfer(lusb_handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE, LIBUSB_REQUEST_GET_DESCRIPTOR, 0x0200 | index, 0, buf, 1000, 0);
		if (ssize < 0)
		{
			sys_usbd.fatal("Couldn't get config for some obscure reason: %d", ssize);
			continue;
		}

		// Minimalistic parse
		auto& conf = device.add_node(UsbDescriptorNode(buf[0], buf[1], &buf[2]));

		for (int index = buf[0]; index < ssize;)
		{
			conf.add_node(UsbDescriptorNode(buf[index], buf[index + 1], &buf[index + 2]));
			index += buf[index];
		}
	}
}

bool usb_device_passthrough::set_configuration(u8 cfg_num)
{
	return (libusb_set_configuration(lusb_handle, cfg_num) == LIBUSB_SUCCESS);
};

bool usb_device_passthrough::set_interface(u8 int_num)
{
	return (libusb_claim_interface(lusb_handle, int_num) == LIBUSB_SUCCESS);
}

void usb_device_passthrough::control_transfer(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u32 buf_size, u8* buf, UsbTransfer& transfer, u32 transfer_id)
{
	if (transfer.transfer == nullptr)
		transfer.transfer = libusb_alloc_transfer(8);

	if (transfer.setup_buf.size() < buf_size + 8)
		transfer.setup_buf.resize(buf_size + 8);

	libusb_fill_control_setup(transfer.setup_buf.data(), bmRequestType, bRequest, wValue, wIndex, buf_size);
	memcpy(transfer.setup_buf.data() + 8, buf, buf_size);
	libusb_fill_control_transfer(transfer.transfer, lusb_handle, transfer.setup_buf.data(), callback_transfer, reinterpret_cast<void *>((u64)transfer_id), 0);
	libusb_submit_transfer(transfer.transfer);
}

void usb_device_passthrough::interrupt_transfer(u32 buf_size, u8 *buf, UsbTransfer& transfer, u32 transfer_id)
{
	// TODO actual endpoint
	libusb_fill_interrupt_transfer(transfer.transfer, lusb_handle, 0x81, buf, buf_size, callback_transfer, reinterpret_cast<void *>((u64)transfer_id), 0);
	libusb_submit_transfer(transfer.transfer);
}

void usb_device_passthrough::isochronous_transfer(UsbTransfer& transfer, u32 transfer_id)
{
	// TODO actual endpoint
	// TODO actual size?
	libusb_fill_iso_transfer(transfer.transfer, lusb_handle, 0x81, (u8 *)transfer.iso_request.buf.get_ptr(), 0xFFFF, transfer.iso_request.num_packets, callback_transfer, reinterpret_cast<void *>((u64)transfer_id), 0);

	for (u32 index = 0; index < transfer.iso_request.num_packets; index++)
	{
		transfer.transfer->iso_packet_desc[index].length = transfer.iso_request.packets[index];
	}

	libusb_submit_transfer(transfer.transfer);
}

//////////////////////////////////////////////////////////////////
// EMULATED DEVICE ///////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
usb_device_emulated::usb_device_emulated(const UsbDeviceDescriptor& _device)
{
	device = UsbDescriptorNode(USB_DESCRIPTOR_DEVICE, _device);
}

bool usb_device_emulated::open_device()
{
	return true;
}

bool usb_device_emulated::set_configuration(u8 cfg_num)
{
	// TODO: check
	current_config = cfg_num;
	return true;
}

bool usb_device_emulated::set_interface(u8 int_num)
{
	// TODO: check
	current_interface = int_num;
	return true;
}

s32 usb_device_emulated::get_descriptor(u8 type, u8 index, u8 *ptr, u32 max_size)
{
	if (type == USB_DESCRIPTOR_STRING)
	{
		if (index < strings.size())
		{
			u8 string_len = (u8)strings[index].size();
			ptr[0] = (string_len * 2) + 2;
			ptr[1] = USB_DESCRIPTOR_STRING;
			for (u32 i = 0; i < string_len; i++)
			{
				ptr[2 + (i * 2)] = strings[index].data()[i];
				ptr[3 + (i * 2)] = 0;
			}
			return (s32)ptr[0];
		}
	}
	else
	{
		sys_usbd.error("[Emulated]: Trying to get a descriptor other than string descriptor");
	}

	return -1;
}

void usb_device_emulated::control_transfer(u8 bmRequestType, u8 bRequest, u16 wValue, u16 wIndex, u16 wLength, u32 buf_size, u8* buf, UsbTransfer& transfer, u32 transfer_id)
{
}

void usb_device_emulated::interrupt_transfer(u32 buf_size, u8 *buf, UsbTransfer& transfer, u32 transfer_id)
{
}

void usb_device_emulated::isochronous_transfer(UsbTransfer& transfer, u32 transfer_id)
{
}

void usb_device_emulated::add_string(char *str)
{
	strings.push_back(str);
}

std::shared_ptr<usb_device> get_ds3()
{
	const auto ds3 = std::make_shared<usb_device_emulated>(UsbDeviceDescriptor{ 0x0200, 0x00, 0x00, 0x00, 0x40, 0x054C, 0x0268, 0x0100, 0x01, 0x02, 0x00, 0x01 });
	auto& config = ds3->device.add_node(UsbDescriptorNode(USB_DESCRIPTOR_CONFIG, UsbDeviceConfiguration{ 0x0029, 0x01, 0x01, 0x00, 0x80, 0xFA }));
	auto& inter = config.add_node(UsbDescriptorNode(USB_DESCRIPTOR_INTERFACE, UsbDeviceInterface{ 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_HID, UsbDeviceHID{ 0x0111, 0x00, 0x01, 0x22, 0x0094 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ENDPOINT, UsbDeviceEndpoint{ 0x02, 0x03, 0x0040, 0x01 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ENDPOINT, UsbDeviceEndpoint{ 0x81, 0x03, 0x0040, 0x01 }));

	ds3->total_size = ds3->device.get_size();

	return ds3;
}

std::shared_ptr<usb_device> get_singstar()
{
	const auto singstar = std::make_shared<usb_device_emulated>(UsbDeviceDescriptor{ 0x0110, 0x00, 0x00, 0x00, 0x08, 0x1415, 0x0000, 0x0001, 0x01, 0x02, 0x00, 0x01 });
	auto& config = singstar->device.add_node(UsbDescriptorNode(USB_DESCRIPTOR_CONFIG, UsbDeviceConfiguration{ 0x00B1, 0x02, 0x01, 0x00, 0x80, 0x2D }));

	auto& inter = config.add_node(UsbDescriptorNode(USB_DESCRIPTOR_INTERFACE, UsbDeviceInterface{ 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceACInterfaceHeader{ 0x01, 0x0100, 0x0028, 0x01, 0x01 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceACInputTerminal{ 0x02, 0x01, 0x0201, 0x02, 0x02, 0x0003, 0x00, 0x00 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceACOutputTerminal{ 0x03, 0x02, 0x0101, 0x01, 0x03, 0x00 }));
	inter.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceACFeatureUnit{ 0x06, 0x03, 0x01, 0x01, 0x01, 0x02, 0x02, 0x00 }));

	config.add_node(UsbDescriptorNode(USB_DESCRIPTOR_INTERFACE, UsbDeviceInterface{ 0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00 }));

	auto& inter3 = config.add_node(UsbDescriptorNode(USB_DESCRIPTOR_INTERFACE, UsbDeviceInterface{ 0x01, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00 }));
	inter3.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceASInterface{ 0x01, 0x02, 0x01, 0x0001 }));
	inter3.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceASFormatType1{ 0x02, 0x01, 0x01, 0x02, 0x10, 0x05, 0x1F40, 0x00, 0x2B11, 0x00, 0x5622, 0x00, 0xAC44, 0x00, 0xBB80, 0x00 }));
	inter3.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ENDPOINT, UsbDeviceEndpointAudio{ 0x81, 0x05, 0x0064, 0x01, 0x00, 0x00 }));
	inter3.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ENDPOINT_ASI, UsbDeviceASIsochronousDataEndpoint{ 0x01, 0x01, 0x00, 0x0000 }));

	auto& inter4 = config.add_node(UsbDescriptorNode(USB_DESCRIPTOR_INTERFACE, UsbDeviceInterface{ 0x01, 0x02, 0x01, 0x01, 0x02, 0x00, 0x00 }));
	inter4.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceASInterface{ 0x01, 0x02, 0x01, 0x0001 }));
	inter4.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ACI, UsbDeviceASFormatType1{ 0x02, 0x01, 0x02, 0x02, 0x10, 0x05, 0x1F40, 0x00, 0x2B11, 0x00, 0x5622, 0x00, 0xAC44, 0x00, 0xBB80, 0x00 }));
	inter4.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ENDPOINT, UsbDeviceEndpointAudio{ 0x81, 0x05, 0x00C8, 0x01, 0x00, 0x00 }));
	inter4.add_node(UsbDescriptorNode(USB_DESCRIPTOR_ENDPOINT_ASI, UsbDeviceASIsochronousDataEndpoint{ 0x01, 0x01, 0x00, 0x0000 }));

	singstar->total_size = singstar->device.get_size();

	singstar->add_string("");
	singstar->add_string("Nam Tai E&E Products Ltd.");
	singstar->add_string("USBMIC Serial# 123456789 ");

	return singstar;
}

void LIBUSB_CALL callback_transfer(struct libusb_transfer *transfer)
{
	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh)
		return;

	usbh->transfer_complete(transfer);
}

usb_handler_thread::usb_handler_thread()
{
	// There always need to be at least one device in the internal list of managed device
	// Otherwise CellUsbd freaks out so we add a fake DS3 to the list
	usb_devices.push_back(get_ds3());

	if (libusb_init(&ctx) < 0)
		return;

	// look if any device which we could be interested in is actually connected
	libusb_device **list;
	ssize_t ndev = libusb_get_device_list(ctx, &list);

	for (ssize_t index = 0; index < ndev; index++)
	{
		libusb_device_descriptor desc;
		libusb_get_device_descriptor(list[index], &desc);

		auto check_device = [&](const u16 id_vendor, const u16 id_product, const char *s_name)
		{
			if (desc.idVendor == id_vendor && desc.idProduct == id_product)
			{
				sys_usbd.notice("Found device: %s", s_name);
				libusb_ref_device(list[index]);
				std::shared_ptr<usb_device_passthrough> usb_dev = std::make_shared<usb_device_passthrough>(list[index], desc);
				usb_devices.push_back(usb_dev);
			}
		};

		check_device(0x1430, 0x0150, "Skylander Portal");
		check_device(0x1415, 0x0000, "Singstar Microphone");
	}

	for (u32 index = 0; index <= 0x43; index++)
	{
		transfers[index].transfer = libusb_alloc_transfer(8);
	}
}

usb_handler_thread::~usb_handler_thread()
{
	for (u32 index = 0; index <= 0x43; index++)
	{
		if (transfers[index].transfer)
			libusb_free_transfer(transfers[index].transfer);
	}
}

void usb_handler_thread::operator()()
{
	timeval lusb_tv;
	memset(&lusb_tv, 0, sizeof(timeval));
	lusb_tv.tv_usec = 200;
	

	while (thread_ctrl::state() != thread_state::aborting && !Emu.IsStopped())
	{
		// Check if any device 
		check_devices_vs_ldds();

		// Process asynchronous requests that are pending
		libusb_handle_events_timeout_completed(ctx, &lusb_tv, nullptr);
	}
}

void usb_handler_thread::transfer_complete(struct libusb_transfer *transfer)
{
	u32 tr_id = (u32)reinterpret_cast<u64>(transfer->user_data);

	if (transfer->status != 0)
	{
		sys_usbd.error("Transfer Error: %d", (s32)transfer->status);
	}

	switch (transfer->status)
	{
	case LIBUSB_TRANSFER_COMPLETED:
		transfers[tr_id].result = HC_CC_NOERR;
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
		transfers[tr_id].result = EHCI_CC_XACT;
		break;
	case LIBUSB_TRANSFER_OVERFLOW:
		transfers[tr_id].result = EHCI_CC_BABBLE;
		break;
	case LIBUSB_TRANSFER_ERROR:
	case LIBUSB_TRANSFER_CANCELLED:
	case LIBUSB_TRANSFER_STALL:
	case LIBUSB_TRANSFER_NO_DEVICE:
	default:
		transfers[tr_id].result = EHCI_CC_HALTED;
		break;
	}

	transfers[tr_id].count = transfer->actual_length;

	for (s32 index = 0; index < transfer->num_iso_packets; index++)
	{
		u8 iso_status;
		switch (transfer->iso_packet_desc[index].status)
		{
		case LIBUSB_TRANSFER_COMPLETED:
			iso_status = USBD_HC_CC_NOERR;
			break;
		case LIBUSB_TRANSFER_TIMED_OUT:
			iso_status = USBD_HC_CC_XACT;
			break;
		case LIBUSB_TRANSFER_OVERFLOW:
			iso_status = USBD_HC_CC_BABBLE;
			break;
		case LIBUSB_TRANSFER_ERROR:
		case LIBUSB_TRANSFER_CANCELLED:
		case LIBUSB_TRANSFER_STALL:
		case LIBUSB_TRANSFER_NO_DEVICE:
		default:
			iso_status = USBD_HC_CC_MISSMF;
			break;
		}

		transfers[tr_id].iso_request.packets[index] = ((iso_status & 0xF) << 12 | (transfer->iso_packet_desc[index].actual_length & 0xFFF));
	}

	transfers[tr_id].busy = false;

	usbd_events.push({ SYS_USBD_TRANSFER_COMPLETE, tr_id, 0x00 });
}

u32 usb_handler_thread::add_ldd(vm::ptr<char> s_product, u16 slen_product, u16 id_vendor, u16 id_product_min, u16 id_product_max)
{
	UsbLdd new_ldd;
	new_ldd.name.resize(slen_product + 1);
	memcpy(new_ldd.name.data(), s_product.get_ptr(), (u32)slen_product);
	new_ldd.id_vendor = id_vendor;
	new_ldd.id_product_min = id_product_min;
	new_ldd.id_product_max = id_product_max;
	ldds.push_back(new_ldd);

	return (u32)ldds.size(); // TODO: to check
}

u32 usb_handler_thread::open_pipe(u32 device_handle, u32 endpoint)
{
	open_pipes.emplace(pipe_counter, UsbPipe{ handled_devices[device_handle].second, endpoint });
	return pipe_counter++;
}

bool usb_handler_thread::close_pipe(u32 pipe_number)
{
	return (bool)open_pipes.erase(pipe_number);
}

void usb_handler_thread::check_devices_vs_ldds()
{
	auto usb_lock = g_idm->lock<named_thread<usb_handler_thread>>(0);

	for (auto& dev : usb_devices)
	{
		for (auto& ldd : ldds)
		{
			if (dev->device._device.idVendor == ldd.id_vendor && dev->device._device.idProduct >= ldd.id_product_min && dev->device._device.idProduct <= ldd.id_product_max && !dev->assigned_number)
			{
				if (!dev->open_device())
				{
					// To avoid spamming log if there is a problem
					if (open_error_counter < 10)
						sys_usbd.error("Failed to open device for LDD(VID:0x%x PID:0x%x)", dev->device._device.idVendor, dev->device._device.idProduct);
					open_error_counter++;
					continue;
				}

				dev->read_descriptors();

				dev->assigned_number = dev_counter;
				handled_devices.emplace(dev_counter, std::pair(UsbInternalDevice{ 0x00, dev_counter, 0x02, 0x40 }, dev ));
				usbd_events.push({ SYS_USBD_ATTACH, dev_counter, 0x00 });
				dev_counter++;
				return;
			}
		}
	}
}

s32 sys_usbd_initialize(vm::ptr<u32> handle)
{
	sys_usbd.warning("sys_usbd_initialize(handle=*0x%x)", handle);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(id_new);

	if (usbh)
	{
		usbh.create("Usb Manager Thread");
	}

	*handle = 0x115B;

	return CELL_OK;
}

s32 sys_usbd_finalize(ppu_thread& ppu, u32 handle)
{
	sys_usbd.warning("sys_usbd_finalize(handle=0x%x)", handle);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);

	if (!usbh)
	{
		return CELL_OK;
	}

	*usbh.get() = thread_state::aborting;
	usbh.unlock();

	while (true)
	{
		if (ppu.is_stopped())
		{
			return 0;
		}

		thread_ctrl::wait_for(1000);

		auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);

		if (*usbh.get() == thread_state::finished)
		{
			usbh.unlock();
			break;
		}
	}

	return CELL_OK;
}

s32 sys_usbd_get_device_list(u32 handle, vm::ptr<UsbInternalDevice> device_list, u32 max_devices)
{
	sys_usbd.warning("sys_usbd_get_device_list(handle=0x%x, device_list=*0x%x, max_devices=0x%x)", handle, device_list, max_devices);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh)
	{
		return CELL_EINVAL;
	}

	for(u32 index = 0; index < max_devices && index < usbh->handled_devices.size(); index++)
	{
		device_list[index] = usbh->handled_devices[index].first;
	}

	return std::min((s32)max_devices, (s32)usbh->handled_devices.size());
}

s32 sys_usbd_register_extra_ldd(u32 handle, vm::ptr<char> s_product, u16 slen_product, u16 id_vendor, u16 id_product_min, u16 id_product_max)
{
	sys_usbd.warning("sys_usbd_register_extra_ldd(handle=0x%x, s_product=%s, slen_product=0x%x, id_vendor=0x%x, id_product_min=0x%x, id_product_max=0x%x)", handle, s_product, slen_product, id_vendor, id_product_min, id_product_max);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh)
	{
		return CELL_EINVAL;
	}

	s32 res = usbh->add_ldd(s_product, slen_product, id_vendor, id_product_min, id_product_max);

	return res; // To check
}

s32 sys_usbd_get_descriptor_size(u32 handle, u32 device_handle)
{
	sys_usbd.trace("sys_usbd_get_descriptor_size(handle=0x%x, deviceNumber=0x%x)", handle, device_handle);
	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->handled_devices.count(device_handle))
	{
		return CELL_EINVAL;
	}
	
	return usbh->handled_devices[device_handle].second->device.get_size();
}

s32 sys_usbd_get_descriptor(u32 handle, u32 device_handle, vm::ptr<void> descriptor, u32 desc_size)
{
	sys_usbd.trace("sys_usbd_get_descriptor(handle=0x%x, deviceNumber=0x%x, descriptor=0x%x, desc_size=0x%x)", handle, device_handle, descriptor, desc_size);
	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->handled_devices.count(device_handle))
	{
		return CELL_EINVAL;
	}

	u8* ptr = (u8 *)descriptor.get_ptr();
	usbh->handled_devices[device_handle].second->device.write_data(ptr);

	return CELL_OK;
}

s32 sys_usbd_register_ldd(u32 handle, vm::ptr<char> s_product, u16 slen_product)
{
	sys_usbd.todo("sys_usbd_register_ldd(handle=0x%x, s_product=%s, slen_product=0x%x, id_vendor=0x%x, id_product=0x%x)", handle, s_product, slen_product);
	return CELL_OK;
}

s32 sys_usbd_unregister_ldd()
{
	sys_usbd.todo("sys_usbd_unregister_ldd()");
	return CELL_OK;
}

s32 sys_usbd_open_pipe(u32 handle, u32 device_handle, u32 endpoint)
{
	sys_usbd.trace("sys_usbd_open_pipe(handle=0x%x, device_handle=0x%x, endpoint=0x%x)", handle, device_handle, endpoint);
	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->handled_devices.count(device_handle))
	{
		return CELL_EINVAL;
	}

	return usbh->open_pipe(device_handle, endpoint);
}

s32 sys_usbd_open_default_pipe(u32 handle, u32 device_handle)
{
	sys_usbd.trace("sys_usbd_open_default_pipe(handle=0x%x, device_handle=0x%x)", handle, device_handle);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->handled_devices.count(device_handle))
	{
		return CELL_EINVAL;
	}

	return usbh->open_pipe(device_handle, 0);
}

s32 sys_usbd_close_pipe(u32 handle, u32 pipe_handle)
{
	sys_usbd.todo("sys_usbd_close_pipe(handle=0x%x, pipe_handle=0x%x)", handle, pipe_handle);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->open_pipes.count(pipe_handle))
	{
		return CELL_EINVAL;
	}

	usbh->close_pipe(pipe_handle);

	return CELL_OK;
}

// From RE:
// In libusbd_callback_thread
// *arg1 = 4 will terminate CellUsbd libusbd_callback_thread
// *arg1 = 3 will do some extra processing right away(notification of transfer finishing)
// *arg1 < 1 || *arg1 > 4 are ignored(rewait instantly for event)
// *arg1 == 1 || *arg1 == 2 will send a sys_event to internal CellUsbd event queue with same parameters as received and loop(attach and detach event)
s32 sys_usbd_receive_event(ppu_thread& ppu, u32 handle, vm::ptr<u64> arg1, vm::ptr<u64> arg2, vm::ptr<u64> arg3)
{
	sys_usbd.trace("sys_usbd_receive_event(handle=%u, arg1=*0x%x, arg2=*0x%x, arg3=*0x%x)", handle, arg1, arg2, arg3);

	while (!Emu.IsStopped())
	{
		{
			auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
			if (!usbh)
			{
				return CELL_EINVAL;
			}

			if (usbh->usbd_events.size())
			{
				auto& usb_event = usbh->usbd_events.front();
				*arg1 = std::get<0>(usb_event);
				*arg2 = std::get<1>(usb_event);
				*arg3 = std::get<2>(usb_event);
				usbh->usbd_events.pop();
				sys_usbd.trace("Sending event: arg1=0x%x arg2=0x%x arg3=0x%x", *arg1, *arg2, *arg3);
				break;
			}
		}

		sys_timer_usleep(ppu, 10000);
	}

	return CELL_OK;
}

s32 sys_usbd_detect_event()
{
	sys_usbd.todo("sys_usbd_detect_event()");
	return CELL_OK;
}

s32 sys_usbd_attach(u32 handle)
{
	sys_usbd.todo("sys_usbd_attach()");
	return CELL_OK;
}

s32 sys_usbd_transfer_data(u32 handle, u32 id_pipe, vm::ptr<void> buf_receive, u32 buf_size, vm::ptr<UsbDeviceRequest> request, u32 type_transfer)
{
	sys_usbd.trace("sys_usbd_transfer_data(handle=0x%x, id_pipe=0x%x, buf_receive=*0x%x, buf_length=0x%x, request=*0x%x, type=0x%x)", handle, id_pipe, buf_receive, buf_size, request, type_transfer);
	if (request.addr())
		sys_usbd.trace("RequestType:0x%x, Request:0x%x, wValue:0x%x, wIndex:0x%x, wLength:0x%x", request->bmRequestType, request->bRequest, request->wValue, request->wIndex, request->wLength);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->open_pipes.count(id_pipe))
	{
		return CELL_EINVAL;
	}

	if (usbh->transfer_counter > 0x43) usbh->transfer_counter = 0;
	// Make sure the current transfer is not busy
	while (usbh->transfers[usbh->transfer_counter].busy)
	{
		usbh->transfer_counter++;
		if (usbh->transfer_counter > 0x43) usbh->transfer_counter = 0;
	}

	auto& pipe = usbh->open_pipes[id_pipe];

	// Default endpoint is control endpoint
	if(pipe.endpoint == 0)
	{
		if (!request.addr())
		{
			sys_usbd.error("Tried to use control pipe without proper request pointer");
			return CELL_EINVAL;
		}

		// Claiming interface
		if (request->bmRequestType == 0 && request->bRequest == 0x09)
		{
			pipe.device->set_configuration((u8)request->wValue);
			pipe.device->set_interface(0);
		}

		pipe.device->control_transfer(request->bmRequestType, request->bRequest, request->wValue, request->wIndex, request->wLength, buf_size, (u8 *)buf_receive.get_ptr(), usbh->transfers[usbh->transfer_counter], usbh->transfer_counter);
		usbh->transfers[usbh->transfer_counter].busy = true;
	}
	else
	{
		pipe.device->interrupt_transfer(buf_size, (u8 *)buf_receive.get_ptr(), usbh->transfers[usbh->transfer_counter], usbh->transfer_counter);
		usbh->transfers[usbh->transfer_counter].busy = true;
	}

	// returns an identifier specific to the transfer(should be <= 0x43)
	return usbh->transfer_counter++;
}

s32 sys_usbd_isochronous_transfer_data(u32 handle, u32 id_pipe, vm::ptr<UsbDeviceIsoRequest> iso_request)
{
	sys_usbd.todo("sys_usbd_isochronous_transfer_data(handle=0x%x, id_pipe=0x%x, iso_request=*0x%x)", handle, id_pipe, iso_request);
	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh || !usbh->open_pipes.count(id_pipe))
	{
		return CELL_EINVAL;
	}

	if (usbh->transfer_counter > 0x43) usbh->transfer_counter = 0;
	// Make sure the current transfer is not busy
	while (usbh->transfers[usbh->transfer_counter].busy)
	{
		usbh->transfer_counter++;
		if (usbh->transfer_counter > 0x43) usbh->transfer_counter = 0;
	}

	auto& pipe = usbh->open_pipes[id_pipe];

	memcpy(&usbh->transfers[usbh->transfer_counter].iso_request, iso_request.get_ptr(), sizeof(UsbDeviceIsoRequest));
	pipe.device->isochronous_transfer(usbh->transfers[usbh->transfer_counter], usbh->transfer_counter);

	// returns an identifier specific to the transfer(should be <= 0x43)
	return usbh->transfer_counter++;
}

s32 sys_usbd_get_transfer_status(u32 handle, u32 id_transfer, u32 unk1, vm::ptr<u32> result, vm::ptr<u32> count)
{
	sys_usbd.trace("sys_usbd_get_transfer_status(handle=0x%x, id_transfer=0x%x, unk1=0x%x, result=*0x%x, count=*0x%x)", handle, id_transfer, unk1, result, count);

	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh)
	{
		return CELL_EINVAL;
	}

	*result = usbh->transfers[id_transfer].result;
	*count = usbh->transfers[id_transfer].count;

	return CELL_OK;
}

s32 sys_usbd_get_isochronous_transfer_status(u32 handle, u32 id_transfer, u32 unk1, vm::ptr<UsbDeviceIsoRequest> request, vm::ptr<u32> result)
{
	sys_usbd.todo("sys_usbd_get_isochronous_transfer_status()");
	auto usbh = g_idm->lock<named_thread<usb_handler_thread>>(0);
	if (!usbh)
	{
		return CELL_EINVAL;
	}

	*result = usbh->transfers[id_transfer].result;
	memcpy(request.get_ptr(), &usbh->transfers[id_transfer].iso_request, sizeof(UsbDeviceIsoRequest));

	return CELL_OK;
}

s32 sys_usbd_get_device_location()
{
	sys_usbd.todo("sys_usbd_get_device_location()");
	return CELL_OK;
}

s32 sys_usbd_send_event()
{
	sys_usbd.todo("sys_usbd_send_event()");
	return CELL_OK;
}

s32 sys_usbd_event_port_send()
{
	sys_usbd.todo("sys_usbd_event_port_send()");
	return CELL_OK;
}

s32 sys_usbd_allocate_memory()
{
	sys_usbd.todo("sys_usbd_allocate_memory()");
	return CELL_OK;
}

s32 sys_usbd_free_memory()
{
	sys_usbd.todo("sys_usbd_free_memory()");
	return CELL_OK;
}

s32 sys_usbd_get_device_speed()
{
	sys_usbd.todo("sys_usbd_get_device_speed()");
	return CELL_OK;
}
