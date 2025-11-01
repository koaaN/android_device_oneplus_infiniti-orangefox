#
#	This file is part of the OrangeFox Recovery Project
# 	Copyright (C) 2020-2025 The OrangeFox Recovery Project
#
#	OrangeFox is free software: you can redistribute it and/or modify
#	it under the terms of the GNU General Public License as published by
#	the Free Software Foundation, either version 3 of the License, or
#	any later version.
#
#	OrangeFox is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#	GNU General Public License for more details.
#
# 	This software is released under GPL version 3 or any later version.
#	See <http://www.gnu.org/licenses/>.
#
# 	Please maintain this if you use this script or any part of it
#
FDEVICE="infiniti"

fox_get_target_device() {
local chkdev=$(echo "$BASH_SOURCE" | grep -w $FDEVICE)
   if [ -n "$chkdev" ]; then 
      FOX_BUILD_DEVICE="$FDEVICE"
   else
      chkdev=$(set | grep BASH_ARGV | grep -w $FDEVICE)
      [ -n "$chkdev" ] && FOX_BUILD_DEVICE="$FDEVICE"
   fi
}

if [ -z "$1" -a -z "$FOX_BUILD_DEVICE" ]; then
   fox_get_target_device
fi

if [ "$1" = "$FDEVICE" -o "$FOX_BUILD_DEVICE" = "$FDEVICE" ]; then
	export LC_ALL="C"
	export FOX_AB_DEVICE=1
	export FOX_USE_TAR_BINARY=1
	export FOX_USE_SED_BINARY=1
	export FOX_USE_LZ4_BINARY=1
	export FOX_USE_ZSTD_BINARY=1
	export FOX_USE_DATE_BINARY=1
	export FOX_DELETE_AROMAFM=1
	export FOX_VANILLA_BUILD=1
	export FOX_USE_GREP_BINARY=1
	export FOX_USE_BUSYBOX_BINARY=1
	export FOX_USE_XZ_UTILS=1
	export FOX_VIRTUAL_AB_DEVICE=1
	export FOX_ALLOW_EARLY_SETTINGS_LOAD=1
	export FOX_USE_UPDATED_MAGISKBOOT=1
	export FOX_MOVE_MAGISK_INSTALLER_TO_RAMDISK=1
	export FOX_USE_FSCK_EROFS_BINARY=1
	export FOX_USE_PATCHELF_BINARY=1

	# For OnePlus 13
	export TARGET_DEVICE_ALT="PLK110,OP60FFL1,CPH2745,CPH2747"
	export FOX_TARGET_DEVICES="$TARGET_DEVICE_ALT"
   	export FOX_USE_DMSETUP=1
	export FOX_ENABLE_KERNELSU_SUPPORT=1
	export FOX_ENABLE_KERNELSU_NEXT_SUPPORT=1
	export FOX_ENABLE_SUKISU_SUPPORT=1
	export FOX_BUILD_TYPE=Beta
fi
#
