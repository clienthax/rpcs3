#include "stdafx.h"
#include "Emu/IdManager.h"
#include "Emu/System.h"
#include "Emu/Cell/PPUModule.h"

#include "Emu/Cell/lv2/sys_event.h"

#include "cellCamera.h"

#include <png.h>

#include <stdio.h>
#include "libuvc/libuvc.h"
//#include "libuvc/libuvc_internal.h"//naughty

logs::channel cellCamera("cellCamera");

template <>
void fmt_class_string<camera_handler>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
	{
		switch (value)
		{
		case camera_handler::null: return "Null";
		case camera_handler::fake: return "Fake";
		}

		return unknown;
	});
}

template <>
void fmt_class_string<fake_camera_type>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
	{
		switch (value)
		{
		case fake_camera_type::unknown: return "Unknown";
		case fake_camera_type::eyetoy: return "EyeToy";
		case fake_camera_type::eyetoy2: return "PS Eye";
		case fake_camera_type::uvc1_1: return "UVC 1.1";
		}

		return unknown;
	});
}

//uvclib callback
void cb(uvc_frame_t *frame, void *ptr) {
	uvc_frame_t *bgr;
	uvc_error_t ret;
	/* We'll convert the image from YUV/JPEG to BGR, so allocate space */
	bgr = uvc_allocate_frame(frame->width * frame->height * 3);
	if (!bgr) {
		printf("unable to allocate bgr frame!");
		return;
	}
	/* Do the BGR conversion */
	ret = uvc_any2bgr(frame, bgr);
	if (ret) {
		uvc_perror(ret, "uvc_any2bgr");
		uvc_free_frame(bgr);
		return;
	}



	/* Call a user function:
	*
	* my_type *my_obj = (*my_type) ptr;
	* my_user_function(ptr, bgr);
	* my_other_function(ptr, bgr->data, bgr->width, bgr->height);
	*/
	/* Call a C++ method:
	*
	* my_type *my_obj = (*my_type) ptr;
	* my_obj->my_func(bgr);
	*/
	/* Use opencv.highgui to display the image:
	*
	* cvImg = cvCreateImageHeader(
	*     cvSize(bgr->width, bgr->height),
	*     IPL_DEPTH_8U,
	*     3);
	*
	* cvSetData(cvImg, bgr->data, bgr->width * 3);
	*
	* cvNamedWindow("Test", CV_WINDOW_AUTOSIZE);
	* cvShowImage("Test", cvImg);
	* cvWaitKey(10);
	*
	* cvReleaseImageHeader(&cvImg);
	*/
	uvc_free_frame(bgr);
}



static const char* get_camera_attr_name(s32 value)
{
	switch (value)
	{
	case CELL_CAMERA_GAIN: return "GAIN";
	case CELL_CAMERA_REDBLUEGAIN: return "REDBLUEGAIN";
	case CELL_CAMERA_SATURATION: return "SATURATION";
	case CELL_CAMERA_EXPOSURE: return "EXPOSURE";
	case CELL_CAMERA_BRIGHTNESS: return "BRIGHTNESS";
	case CELL_CAMERA_AEC: return "AEC";
	case CELL_CAMERA_AGC: return "AGC";
	case CELL_CAMERA_AWB: return "AWB";
	case CELL_CAMERA_ABC: return "ABC";
	case CELL_CAMERA_LED: return "LED";
	case CELL_CAMERA_AUDIOGAIN: return "AUDIOGAIN";
	case CELL_CAMERA_QS: return "QS";
	case CELL_CAMERA_NONZEROCOEFFS: return "NONZEROCOEFFS";
	case CELL_CAMERA_YUVFLAG: return "YUVFLAG";
	case CELL_CAMERA_JPEGFLAG: return "JPEGFLAG";
	case CELL_CAMERA_BACKLIGHTCOMP: return "BACKLIGHTCOMP";
	case CELL_CAMERA_MIRRORFLAG: return "MIRRORFLAG";
	case CELL_CAMERA_MEASUREDQS: return "MEASUREDQS";
	case CELL_CAMERA_422FLAG: return "422FLAG";
	case CELL_CAMERA_USBLOAD: return "USBLOAD";
	case CELL_CAMERA_GAMMA: return "GAMMA";
	case CELL_CAMERA_GREENGAIN: return "GREENGAIN";
	case CELL_CAMERA_AGCLIMIT: return "AGCLIMIT";
	case CELL_CAMERA_DENOISE: return "DENOISE";
	case CELL_CAMERA_FRAMERATEADJUST: return "FRAMERATEADJUST";
	case CELL_CAMERA_PIXELOUTLIERFILTER: return "PIXELOUTLIERFILTER";
	case CELL_CAMERA_AGCLOW: return "AGCLOW";
	case CELL_CAMERA_AGCHIGH: return "AGCHIGH";
	case CELL_CAMERA_DEVICELOCATION: return "DEVICELOCATION";
	case CELL_CAMERA_FORMATCAP: return "FORMATCAP";
	case CELL_CAMERA_FORMATINDEX: return "FORMATINDEX";
	case CELL_CAMERA_NUMFRAME: return "NUMFRAME";
	case CELL_CAMERA_FRAMEINDEX: return "FRAMEINDEX";
	case CELL_CAMERA_FRAMESIZE: return "FRAMESIZE";
	case CELL_CAMERA_INTERVALTYPE: return "INTERVALTYPE";
	case CELL_CAMERA_INTERVALINDEX: return "INTERVALINDEX";
	case CELL_CAMERA_INTERVALVALUE: return "INTERVALVALUE";
	case CELL_CAMERA_COLORMATCHING: return "COLORMATCHING";
	case CELL_CAMERA_PLFREQ: return "PLFREQ";
	case CELL_CAMERA_DEVICEID: return "DEVICEID";
	case CELL_CAMERA_DEVICECAP: return "DEVICECAP";
	case CELL_CAMERA_DEVICESPEED: return "DEVICESPEED";
	case CELL_CAMERA_UVCREQCODE: return "UVCREQCODE";
	case CELL_CAMERA_UVCREQDATA: return "UVCREQDATA";
	case CELL_CAMERA_DEVICEID2: return "DEVICEID2";
	case CELL_CAMERA_READMODE: return "READMODE";
	case CELL_CAMERA_GAMEPID: return "GAMEPID";
	case CELL_CAMERA_PBUFFER: return "PBUFFER";
	case CELL_CAMERA_READFINISH: return "READFINISH";
	}

	return nullptr;
}

// Custom struct to keep track of cameras
struct camera_t
{
	struct attr_t
	{
		u32 v1, v2;
	};

	std::vector<u64> keys;

	attr_t attr[500]{};
};

png_bytep * row_pointers;
u32 cameraStatus = 0;

s32 cellCameraInit()
{
	cellCamera.warning("cellCameraInit()");

	if (g_cfg.io.camera == camera_handler::null)
	{
		return CELL_CAMERA_ERROR_DEVICE_NOT_FOUND;
	}

	const auto camera = fxm::make<camera_t>();

	if (!camera)
	{
		return CELL_CAMERA_ERROR_ALREADY_INIT;
	}

	switch (g_cfg.io.camera_type)
	{
	case fake_camera_type::eyetoy:
	{
		camera->attr[CELL_CAMERA_SATURATION] = { 164 };
		camera->attr[CELL_CAMERA_BRIGHTNESS] = { 96 };
		camera->attr[CELL_CAMERA_AEC] = { 1 };
		camera->attr[CELL_CAMERA_AGC] = { 1 };
		camera->attr[CELL_CAMERA_AWB] = { 1 };
		camera->attr[CELL_CAMERA_ABC] = { 0 };
		camera->attr[CELL_CAMERA_LED] = { 1 };
		camera->attr[CELL_CAMERA_QS] = { 0 };
		camera->attr[CELL_CAMERA_NONZEROCOEFFS] = { 32, 32 };
		camera->attr[CELL_CAMERA_YUVFLAG] = { 0 };
		camera->attr[CELL_CAMERA_BACKLIGHTCOMP] = { 0 };
		camera->attr[CELL_CAMERA_MIRRORFLAG] = { 1 };
		camera->attr[CELL_CAMERA_422FLAG] = { 1 };
		camera->attr[CELL_CAMERA_USBLOAD] = { 4 };
		break;
	}

	case fake_camera_type::eyetoy2:
	{
		camera->attr[CELL_CAMERA_SATURATION] = { 64 };
		camera->attr[CELL_CAMERA_BRIGHTNESS] = { 8 };
		camera->attr[CELL_CAMERA_AEC] = { 1 };
		camera->attr[CELL_CAMERA_AGC] = { 1 };
		camera->attr[CELL_CAMERA_AWB] = { 1 };
		camera->attr[CELL_CAMERA_LED] = { 1 };
		camera->attr[CELL_CAMERA_BACKLIGHTCOMP] = { 0 };
		camera->attr[CELL_CAMERA_MIRRORFLAG] = { 1 };
		camera->attr[CELL_CAMERA_GAMMA] = { 1 };
		camera->attr[CELL_CAMERA_AGCLIMIT] = { 4 };
		camera->attr[CELL_CAMERA_DENOISE] = { 0 };
		camera->attr[CELL_CAMERA_FRAMERATEADJUST] = { 0 };
		camera->attr[CELL_CAMERA_PIXELOUTLIERFILTER] = { 1 };
		camera->attr[CELL_CAMERA_AGCLOW] = { 48 };
		camera->attr[CELL_CAMERA_AGCHIGH] = { 64 };
		break;
	}
	case fake_camera_type::uvc1_1:
	{
		//Guessing here :)
		cellCamera.todo("Trying to init cellCamera with un-researched camera type.");

		camera->attr[CELL_CAMERA_FORMATCAP] = { 1 << CELL_CAMERA_JPG };
		camera->attr[CELL_CAMERA_NUMFRAME] = { 1 };
		camera->attr[CELL_CAMERA_FRAMESIZE] = { 640, 480 };//vga, This should be indexed depending on the value of frameindex~
		camera->attr[CELL_CAMERA_INTERVALTYPE] = { 0 };
		camera->attr[CELL_CAMERA_INTERVALVALUE] = { 1 * 1000 * 1000 * 10 };//This should be indexed depending on the value of intervalindex~

		break;
	}
	default:
		cellCamera.todo("Trying to init cellCamera with un-researched camera type.");
	}

	// TODO: Some other default attributes? Need to check the actual behaviour on a real PS3.


	//Dodgy png load

	int x, y;

	int width, height;
	png_byte color_type;
	png_byte bit_depth;

	png_structp png_ptr;
	png_infop info_ptr;
	int number_of_passes;

	char header[8];    // 8 is the maximum size that can be checked

					   /* open file and test for it being a png */
	FILE *fp = fopen("C:\\Users\\clienthax\\Downloads\\remsmall.png", "rb");
	if (!fp)
		cellCamera.todo("[read_png_file] File could not be opened for reading");
	fread(header, 1, 8, fp);


	/* initialize stuff */
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (!png_ptr)
		cellCamera.todo("[read_png_file] png_create_read_struct failed");

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		cellCamera.todo("[read_png_file] png_create_info_struct failed");

	if (setjmp(png_jmpbuf(png_ptr)))
		cellCamera.todo("[read_png_file] Error during init_io");

	png_init_io(png_ptr, fp);
	png_set_sig_bytes(png_ptr, 8);

	png_read_info(png_ptr, info_ptr);

	width = png_get_image_width(png_ptr, info_ptr);
	height = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth = png_get_bit_depth(png_ptr, info_ptr);

	number_of_passes = png_set_interlace_handling(png_ptr);
	png_read_update_info(png_ptr, info_ptr);


	/* read file */
	if (setjmp(png_jmpbuf(png_ptr)))
		cellCamera.todo("[read_png_file] Error during read_image");

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	for (y = 0; y<height; y++)
		row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));

	png_read_image(png_ptr, row_pointers);

	fclose(fp);
	//end of dodgy png load

	//uvclib test code
	uvc_context_t *ctx;
	uvc_device_t *dev;
	uvc_device_handle_t *devh;
	uvc_stream_ctrl_t ctrl;
	uvc_error_t res;

	/* Initialize a UVC service context. Libuvc will set up its own libusb
	* context. Replace NULL with a libusb_context pointer to run libuvc
	* from an existing libusb context. */
	res = uvc_init(&ctx, NULL);
	if (res < 0) {
		cellCamera.todo("uvc_init");
		return res;
	}
	cellCamera.todo("UVC initialized");
	/* Locates the first attached UVC device, stores in dev */
	res = uvc_find_device(
		ctx, &dev,
		0x046d, 0x082d, NULL); /* filter devices: vendor_id, product_id, "serial_num" */
	if (res < 0) {
		cellCamera.todo("uvc_find_device no devices"); /* no devices found */
	}
	else {
		cellCamera.todo("Device found res=%d", (int)res);
		/* Try to open the device: requires exclusive access */
		res = uvc_open(dev, &devh);
		if (res < 0) {
			cellCamera.todo("uvc_open unable to open device err=%d", (int)res); /* unable to open device */
		}
		else {
			cellCamera.todo("Device opened");
			/* Print out a message containing all the information that libuvc
			* knows about the device */
			//cellCamera.todo("%s", devh);
			
			/* Try to negotiate a 640x480 30 fps YUYV stream profile */
			//res = uvc_get_stream_ctrl_format_size(
			//	devh, &ctrl, /* result stored in ctrl */
			//	UVC_FRAME_FORMAT_YUYV, /* YUV 422, aka YUV 4:2:2. try _COMPRESSED */
			//	640, 480, 30 /* width, height, fps */
			//);

			res = uvc_get_stream_ctrl_format_size(
				devh, &ctrl, /* result stored in ctrl */
				UVC_FRAME_FORMAT_YUYV, /* YUV 422, aka YUV 4:2:2. try _COMPRESSED */
				640, 480, 30 /* width, height, fps */
			);

			cellCamera.todo("ctrl format res=%d", (int)res);


			/* Print out the result */
 			uvc_print_stream_ctrl(&ctrl, stderr);

			if (res < 0) {
				cellCamera.todo("device doesn't provide a matching stream"); /* device doesn't provide a matching stream */
			}
			else {
				/* Start the video stream. The library will call user function cb:
				*   cb(frame, (void*) 12345)
				*/
				res = uvc_start_streaming(devh, &ctrl, cb, NULL, 0);
				if (res < 0) {
					cellCamera.todo("unable to start stream res=%d", (int)res); /* unable to start stream */
				}
				else {
					cellCamera.todo("Streaming...");
					uvc_set_ae_mode(devh, 1); /* e.g., turn on auto exposure */
					//sleep(10); /* stream for 10 seconds */
							   /* End the stream. Blocks until last callback is serviced */
					uvc_stop_streaming(devh);
					cellCamera.todo("Done streaming.");
				}
			}
			/* Release our handle on the device */
			uvc_close(devh);
			cellCamera.todo("Device closed");
		}
		/* Release the device descriptor */
		uvc_unref_device(dev);
	}
	/* Close the UVC context. This closes and cleans up any existing device handles,
	* and it closes the libusb context if one was not provided. */
	uvc_exit(ctx);
	cellCamera.todo("UVC exited");


	return CELL_OK;
}

s32 cellCameraEnd()
{
	cellCamera.warning("cellCameraEnd()");

	if (!fxm::remove<camera_t>())
	{
		return CELL_CAMERA_ERROR_NOT_INIT;
	}

	return CELL_OK;
}

s32 cellCameraOpen()
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraOpenEx(s32 dev_num, vm::ptr<CellCameraInfoEx> info)
{
	cameraStatus = 1;

	cellCamera.todo("cellCameraOpenEx(dev_num=%d, info=*0x%x)", dev_num, info);
	cellCamera.todo("format %d", info->format);
	cellCamera.todo("resolution %d", info->resolution);
	cellCamera.todo("framerate %d", info->framerate);
	cellCamera.todo("container %d", info->container);
	cellCamera.todo("read_mode %d", info->read_mode);
	//Only the above are filled in by the calling app

	//Test crap
	info->width = 640;
	info->height = 480;
	info->dev_num = 0;
	info->guid = 100;

	//TODO make this a sane size fo
	info->bytesize = info->width * info->height * 4;//sane size?
	info->buffer.set(vm::alloc(info->bytesize, vm::main));
	cellCamera.todo("buffer 0x%x", info->buffer);

	//Fill with junks
	//Eyetoy 2 type is raw8 (pseye)
	//png -> raw8.. scary code but 'works'
	u8 r = 0;
	u8 g = 0;
	u8 b = 0;

	int w = info->width;
	int w2 = info->width * 2;
	int w4 = info->width * 4;

	unsigned char* sptr = reinterpret_cast<unsigned char*>(info->buffer.get_ptr());

	//Black magic
	for (int j = 0; j < info->height; j += 2) {
		for (int i = 0; i < info->width; i += 2) {

			//dodgy png read
			png_byte* row = row_pointers[j];
			png_byte* ptr = &(row[i * 4]);
			r = ptr[0];
			g = ptr[1];
			b = ptr[2];
			//end of dodgy png read

			sptr[w + 1] = r;//red 11

			sptr[0] = b;
			sptr[2] = b;
			sptr[w2] = b;
			sptr[w2+2] = b;

			sptr[1] = b;
			sptr[w] = b;
			sptr[w + 2] = b;
			sptr[w2 + 1] = b;
			sptr -= w + 1;

			sptr[0] = r;
			sptr[2] = r;
			sptr[w2] = r;
			sptr[w2 + 2] = r;

			sptr[1] = g;
			sptr[w] = g;
			sptr[w + 2] = g;
			sptr[w2 + 1] = g;

			sptr += w + 1;


			sptr += 2;


		}
		sptr += w;
	}

	return CELL_OK;
}

s32 cellCameraClose(s32 dev_num)
{
	cameraStatus = 0;
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraGetDeviceGUID(s32 dev_num, vm::ptr<u32> guid)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraGetType(s32 dev_num, vm::ptr<s32> type)
{
	cellCamera.warning("cellCameraGetType(dev_num=%d, type=*0x%x)", dev_num, type);

	const auto camera = fxm::get<camera_t>();

	if (!camera)
	{
		return CELL_CAMERA_ERROR_NOT_INIT;
	}

	switch (g_cfg.io.camera_type)
	{
	case fake_camera_type::unknown: *type = CELL_CAMERA_TYPE_UNKNOWN; break;
	case fake_camera_type::eyetoy:  *type = CELL_CAMERA_EYETOY; break;
	case fake_camera_type::eyetoy2: *type = CELL_CAMERA_EYETOY2; break;
	case fake_camera_type::uvc1_1:  *type = CELL_CAMERA_USBVIDEOCLASS; break;
	}

	return CELL_OK;
}

s32 cellCameraIsAvailable(s32 dev_num)
{
	cellCamera.warning("cellCameraIsAvailable(dev_num=%d)", dev_num);
	if (g_cfg.io.camera == camera_handler::fake)
	{
		return 1;
	}

	return 0;//bool
}

s32 cellCameraIsAttached(s32 dev_num)
{
	cellCamera.warning("cellCameraIsAttached(dev_num=%d)", dev_num);

	if (g_cfg.io.camera == camera_handler::fake)
	{
		return 1;
	}

	return 0; // It's not CELL_OK lol
}

s32 cellCameraIsOpen(s32 dev_num)
{
	cellCamera.todo("cellCameraIsOpen(dev_num=%d)", dev_num);
	return cameraStatus;
}

s32 cellCameraIsStarted(s32 dev_num)
{
	cellCamera.todo("cellCameraIsStarted(dev_num=%d)", dev_num);
	return CELL_OK;
}

s32 cellCameraGetAttribute(s32 dev_num, s32 attrib, vm::ptr<u32> arg1, vm::ptr<u32> arg2)
{
	cellCamera.warning("cellCameraGetAttribute(dev_num=%d, attrib=%d, arg1=*0x%x, arg2=*0x%x)", dev_num, attrib, arg1, arg2);

	const auto attr_name = get_camera_attr_name(attrib);

	const auto camera = fxm::get<camera_t>();

	if (!camera)
	{
		return CELL_CAMERA_ERROR_NOT_INIT;
	}

	if (!attr_name) // invalid attributes don't have a name
	{
		return CELL_CAMERA_ERROR_PARAM;
	}

	cellCamera.warning("attrib name = %s", attr_name);


	if(arg1 != vm::null)
		*arg1 = camera->attr[attrib].v1;
	if(arg2 != vm::null)
		*arg2 = camera->attr[attrib].v2;

	return CELL_OK;
}

s32 cellCameraSetAttribute(s32 dev_num, s32 attrib, u32 arg1, u32 arg2)
{
	cellCamera.warning("cellCameraSetAttribute(dev_num=%d, attrib=%d, arg1=%d, arg2=%d)", dev_num, attrib, arg1, arg2);

	const auto attr_name = get_camera_attr_name(attrib);

	const auto camera = fxm::get<camera_t>();

	if (!camera)
	{
		return CELL_CAMERA_ERROR_NOT_INIT;
	}

	if (!attr_name) // invalid attributes don't have a name
	{
		return CELL_CAMERA_ERROR_PARAM;
	}
	cellCamera.warning("attrib name = %s", attr_name);


	camera->attr[attrib] = { arg1, arg2 };

	return CELL_OK;
}

s32 cellCameraGetBufferSize(s32 dev_num, vm::ptr<CellCameraInfoEx> info)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraGetBufferInfo()
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraGetBufferInfoEx(s32 dev_num, vm::ptr<CellCameraInfoEx> info)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraPrepExtensionUnit(s32 dev_num, vm::ptr<u8> guidExtensionCode)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraCtrlExtensionUnit(s32 dev_num, u8 request, u16 value, u16 length, vm::ptr<u8> data)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraGetExtensionUnit(s32 dev_num, u16 value, u16 length, vm::ptr<u8> data)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraSetExtensionUnit(s32 dev_num, u16 value, u16 length, vm::ptr<u8> data)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraReset(s32 dev_num)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraStart(s32 dev_num)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraRead(s32 dev_num, vm::ptr<u32> frame_num, vm::ptr<u32> bytes_read)
{
	cellCamera.todo("cellCameraRead(dev_num=%d, frame_num=0x%x, bytes_read=0x%x)", dev_num, frame_num, bytes_read);
	//TODO
	*bytes_read = 680*480*4;
	*frame_num = 0;


	return CELL_OK;
}

s32 cellCameraReadEx(s32 dev_num, vm::ptr<CellCameraReadEx> read)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraReadComplete(s32 dev_num, u32 bufnum, u32 arg2)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraStop(s32 dev_num)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraSetNotifyEventQueue(u64 key)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraRemoveNotifyEventQueue(u64 key)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

s32 cellCameraSetNotifyEventQueue2(u64 key, u64 source, u64 flag)//flag is an event flag
{
	cellCamera.todo("cellCameraSetNotifyEventQueue2(key=0x%x, source=0x%x, flag=0x%x)", key, source, flag);

	const auto camera = fxm::get<camera_t>();

	if (!camera)
	{
		return CELL_CAMERA_ERROR_NOT_INIT;
	}

	for (auto k : camera->keys)
	{
		if (k == key)
		{
			return CELL_CAMERA_ERROR_FATAL;
		}
	}

	camera->keys.emplace_back(key);

	//Not the right place for this
	if (auto queue = lv2_event_queue::find(key))
	{
		queue->send(source, 1, 0, 0);//CELL_CAMERA_ATTACH, deviceId, depends on read_mode
	}
	if (auto queue = lv2_event_queue::find(key))
	{
		//queue->send(source, 2, 0, 0);//CELL_CAMERA_FRAME_UPDATE, deviceId, depends on read_mode
	}
	

	return CELL_OK;
}

s32 cellCameraRemoveNotifyEventQueue2(u64 key)
{
	UNIMPLEMENTED_FUNC(cellCamera);
	return CELL_OK;
}

DECLARE(ppu_module_manager::cellCamera)("cellCamera", []()
{
	REG_FUNC(cellCamera, cellCameraInit);
	REG_FUNC(cellCamera, cellCameraEnd);
	REG_FUNC(cellCamera, cellCameraOpen); // was "renamed", but exists
	REG_FUNC(cellCamera, cellCameraOpenEx);
	REG_FUNC(cellCamera, cellCameraClose);

	REG_FUNC(cellCamera, cellCameraGetDeviceGUID);
	REG_FUNC(cellCamera, cellCameraGetType);
	REG_FUNC(cellCamera, cellCameraIsAvailable);
	REG_FUNC(cellCamera, cellCameraIsAttached);
	REG_FUNC(cellCamera, cellCameraIsOpen);
	REG_FUNC(cellCamera, cellCameraIsStarted);
	REG_FUNC(cellCamera, cellCameraGetAttribute);
	REG_FUNC(cellCamera, cellCameraSetAttribute);
	REG_FUNC(cellCamera, cellCameraGetBufferSize);
	REG_FUNC(cellCamera, cellCameraGetBufferInfo); // was "renamed", but exists
	REG_FUNC(cellCamera, cellCameraGetBufferInfoEx);

	REG_FUNC(cellCamera, cellCameraPrepExtensionUnit);
	REG_FUNC(cellCamera, cellCameraCtrlExtensionUnit);
	REG_FUNC(cellCamera, cellCameraGetExtensionUnit);
	REG_FUNC(cellCamera, cellCameraSetExtensionUnit);

	REG_FUNC(cellCamera, cellCameraReset);
	REG_FUNC(cellCamera, cellCameraStart);
	REG_FUNC(cellCamera, cellCameraRead);
	REG_FUNC(cellCamera, cellCameraReadEx);
	REG_FUNC(cellCamera, cellCameraReadComplete);
	REG_FUNC(cellCamera, cellCameraStop);

	REG_FUNC(cellCamera, cellCameraSetNotifyEventQueue);
	REG_FUNC(cellCamera, cellCameraRemoveNotifyEventQueue);
	REG_FUNC(cellCamera, cellCameraSetNotifyEventQueue2);
	REG_FUNC(cellCamera, cellCameraRemoveNotifyEventQueue2);
});
