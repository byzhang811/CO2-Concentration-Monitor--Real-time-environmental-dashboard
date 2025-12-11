# CO2-Concentration-Monitor--Real-time-environmental-dashboard
EC535 Final Project(Lab5)
A small **CO₂ concentration monitor** running on **BeagleBone Black** with a **Qt touchscreen UI**, **GPIO LEDs**, and **CSV logging**.  
Developed for **BU EC535 Lab 5**.
## Features

- Real-time CO₂ display (ppm) with simple status levels (Good/Fair/Moderate/Poor)
- Full-screen, touch-friendly Qt Widgets UI
- 60-second CO₂ trend plot with min / avg / max
- Red / green LEDs via GPIO to indicate high / normal CO₂
- Periodic logging to `/root/co2_log.csv` for offline analysis
- Screen saver with “touch to wake” when idle

## Hardware

- **Board**: BeagleBone Black
- **Display**: EC535 LCD touchscreen cape (framebuffer `/dev/fb0`)
- **Sensor**: CCS811 CO₂ sensor over I²C  
- **LEDs**:
  - Red LED on **GPIO 60**
  - Green LED on **GPIO 48**

> Note: The program accesses `/dev/fb0`, `/sys/class/gpio/...` and `/root/co2_log.csv`, so it should run as **root**.
> chmod -x ./my_qt_app
> chmod -x ./fix_net_and_sync_time.sh

## Build

### Using existing Makefile (EC535 toolchain)

make
This produces the ARM binary my_qt_app.

Using qmake

If you have the correct Qt + cross-toolchain:

qmake my_qt_app.pro

On a desktop with Qt 5 you can also build and run for debugging, but GPIO / I²C will not work without stubs.

Run on BeagleBone

Copy the binary to the BeagleBone (e.g., /root), then:

cd /root
./my_qt_app -platform linuxfb


Tap Exit to quit.

CO₂ readings are logged to /root/co2_log.csv.

Repository Layout
```bash
.
├── main.cpp        # Qt GUI: dashboard, trend plot, screen saver, GPIO, logging
├── ccs811_qt.c     # CCS811 sensor driver (I²C)
├── my_qt_app.pro   # qmake project file
├── Makefile        # Build file for EC535 cross toolchain
├── lab5.pptx       # Lab 5 slides / design overview
└── README.md       # Project description (this file)
