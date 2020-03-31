#!/bin/sh

ln -s /sys/devices/platform/vkeypanel.0/TX_DATA /dev/input/by-path/vkeypanel_TX_DATA
ln -s /sys/devices/platform/vkeypanel.0/RX_DATA /dev/input/by-path/vkeypanel_RX_DATA
chgrp kdev /sys/devices/platform/vkeypanel.0/TX_DATA
chgrp kdev /dev/input/by-path/vkeypanel_TX_DATA
chgrp kdev /sys/devices/platform/vkeypanel.0/RX_DATA
chgrp kdev /dev/input/by-path/vkeypanel_RX_DATA
