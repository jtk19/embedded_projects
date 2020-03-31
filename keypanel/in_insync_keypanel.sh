#!/bin/sh

ln -s /sys/devices/platform/insync_keypanel.0/TX_DATA /dev/input/by-path/insync_keypanel_TX_DATA
chgrp product /sys/devices/platform/insync_keypanel.0/TX_DATA
chgrp producr /dev/input/by-path/insync_keypanel_TX_DATA

