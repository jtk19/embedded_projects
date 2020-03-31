#!/bin/sh
#-----------------------------------------------------------------------------
# Name: 	/etc/insync/in_insync_adcc.sh
# Author: 	Taxila
# Date:		September 2011
# Description: Script to load module insync_adcc.ko with the module parameter
#-----------------------------------------------------------------------------

IQP_CHANNEL_MASK=0xF7	# 1111 0111
HDS_CHANNEL_MASK=0x0E	# 0000 1110
FRC_CHANNEL_MASK=0xFF	# 1111 1111

dev=$HOSTNAME

if [[ "$dev" == "iqp"  ||  "$dev" == "xIQP" ]]; then
	echo "loading insync_adcc.ko for the $dev device with $IQP_CHANNEL_MASK mask"
	/sbin/modprobe --ignore-install insync_adcc adc_channel_mask=$IQP_CHANNEL_MASK
elif [[ "$dev" == "hds"  ||  "$dev" == "HDS" ]]; then
	echo "loading insync_adcc.ko for the $dev device with $HDS_CHANNEL_MASK mask"
	/sbin/modprobe --ignore-install insync_adcc adc_channel_mask=$HDS_CHANNEL_MASK
else # default device
	echo "loading insync_adcc.ko for the $dev device with $FRC_CHANNEL_MASK mask"
	/sbin/modprobe --ignore-install insync_adcc adc_channel_mask=$FRC_CHANNEL_MASK
fi

