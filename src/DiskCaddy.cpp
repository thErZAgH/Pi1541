// Pi1541 - A Commodore 1541 disk drive emulator
// Copyright(C) 2018 Stephen White
//
// This file is part of Pi1541.
// 
// Pi1541 is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Pi1541 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Pi1541. If not, see <http://www.gnu.org/licenses/>.

#include "DiskCaddy.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include "ff.h"
extern "C"
{
#include "rpi-gpio.h"	// For SetACTLed
}

static const u32 screenPosXCaddySelections = 240;
static const u32 screenPosYCaddySelections = 280;
static char buffer[256] = { 0 };
static u32 white = RGBA(0xff, 0xff, 0xff, 0xff);
static u32 red = RGBA(0xff, 0, 0, 0xff);

bool DiskCaddy::Insert(const FILINFO* fileInfo, bool readOnly)
{
	bool success;
	FIL fp;
	FRESULT res = f_open(&fp, fileInfo->fname, FA_READ);
	if (res == FR_OK)
	{
		if (screen)
		{
			int x = screen->ScaleX(screenPosXCaddySelections);
			int y = screen->ScaleY(screenPosYCaddySelections);

			snprintf(buffer, 256, "Loading %s\r\n", fileInfo->fname);
			screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
		}

		u32 bytesRead;
		SetACTLed(true);
		f_read(&fp, DiskImage::readBuffer, READBUFFER_SIZE, &bytesRead);
		SetACTLed(false);
		f_close(&fp);

		DiskImage::DiskType diskType = DiskImage::GetDiskImageTypeViaExtention(fileInfo->fname);
		switch (diskType)
		{
			case DiskImage::D64:
				success = InsertD64(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::G64:
				success = InsertG64(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::NIB:
				success = InsertNIB(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::NBZ:
				success = InsertNBZ(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			default:
				success = false;
				break;
		}
		if (success)
		{
			DEBUG_LOG("Mounted into caddy %s - %d\r\n", fileInfo->fname, bytesRead);
		}
	}
	else
	{
		DEBUG_LOG("Failed to open %s\r\n", fileInfo->fname);
		success = false;
	}

	oldCaddyIndex = 0;

	return success;
}

bool DiskCaddy::InsertD64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenD64(fileInfo, diskImageData, size))
	{
		diskImage.SetReadOnly(readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertG64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenG64(fileInfo, diskImageData, size))
	{
		diskImage.SetReadOnly(readOnly);
		disks.push_back(diskImage);
		//DEBUG_LOG("Disks size = %d\r\n", disks.size());
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertNIB(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenNIB(fileInfo, diskImageData, size))
	{
		// At the moment we cannot write out NIB files.
		diskImage.SetReadOnly(true);// readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertNBZ(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenNBZ(fileInfo, diskImageData, size))
	{
		// At the moment we cannot write out NIB files.
		diskImage.SetReadOnly(true);// readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

void DiskCaddy::Display()
{
	if (screen)
	{
		unsigned numberOfImages = GetNumberOfImages();
		unsigned caddyIndex;
		int x = screen->ScaleX(screenPosXCaddySelections);
		int y = screen->ScaleY(screenPosYCaddySelections);

		snprintf(buffer, 256, "Emulating\r\n");
		screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
		y += 16;

		for (caddyIndex = 0; caddyIndex < numberOfImages; ++caddyIndex)
		{
			DiskImage* image = GetImage(caddyIndex);
			const char* name = image->GetName();
			if (name)
			{
				snprintf(buffer, 256, "%d %s\r\n", caddyIndex + 1, name);
				screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
				y += 16;
			}
		}

		ShowSelectedImage(0);
	}
}

void DiskCaddy::ShowSelectedImage(u32 index)
{
	u32 x = screen->ScaleX(screenPosXCaddySelections) - 16;
	u32 y = screen->ScaleY(screenPosYCaddySelections) + 16 + 16 * index;
	snprintf(buffer, 256, "*");
	screen->PrintText(false, x, y, buffer, white, red);
}

bool DiskCaddy::Update()
{
	u32 y;
	u32 x;
	u32 caddyIndex = GetSelectedIndex();
	if (caddyIndex != oldCaddyIndex)
	{
		if (screen)
		{
			x = screen->ScaleX(screenPosXCaddySelections) - 16;
			y = screen->ScaleY(screenPosYCaddySelections) + 16 + 16 * oldCaddyIndex;
			snprintf(buffer, 256, " ");
			screen->PrintText(false, x, y, buffer, red, red);
			oldCaddyIndex = caddyIndex;
			ShowSelectedImage(oldCaddyIndex);
		}

		return true;
	}
	return false;
}
