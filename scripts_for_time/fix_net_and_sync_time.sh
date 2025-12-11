#!/bin/sh
# Fix BBB network and sync time from Windows host

echo "[1] Configuring network..."
ifconfig eth0 192.168.137.2 netmask 255.255.255.0 up
route add default gw 192.168.137.1 eth0 2>/dev/null

echo "nameserver 8.8.8.8" > /etc/resolv.conf

echo "[2] Checking network..."
ping -c 1 192.168.137.1 >/dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "ERROR: Cannot reach 192.168.137.1"
    exit 1
fi
echo "Network OK."

echo "[3] Fetching time..."
TIME=$(wget -qO- http://192.168.137.1:8000/time.txt | tr -d '\r')

if [ -z "$TIME" ]; then
    echo "ERROR: Failed to retrieve time."
    exit 1
fi

echo "Time received: $TIME"

echo "[4] Applying system time..."
date -s "$TIME"
hwclock -w || echo "Warning: hwclock update failed (no RTC or permission)."

echo "Time sync completed."
date

