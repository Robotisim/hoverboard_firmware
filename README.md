### Decisions
- We have configured the [Config file](software/firmwares/hoverboard_stm32/Inc/config.h) for below features
  - We are using speed mode in Control Mode : #define CTRL_MOD_REQ    SPD_MODE
    - Because speed is feedback based and stm tries to achieve it . Voltage is not desired parameter to attain
  - INACTIVITY_TIMEOUT =0 : so not turning off the motors after 8 mins
  - Odomtery is enabled : #define ENABLE_ODOMETRY
  - Buzer disabled
- Its taking Input as RPM

### How to flash it
- Clone repository
```bash
git@github.com:Robotisim/hoverboard_firmware.git
```

## Install tools
```bash
pip install platformio
pip install pyserial
sudo apt install stlink-tools
sudo apt install openocd
```
- Check versions

```bash
pio --version
st-flash --version
```

### Serial and USB permissions

Skip if flashing and opening the port already work without `sudo`.

```bash
sudo usermod -aG dialout $USER          # for /dev/ttyACM0, then log out and back in

echo 'SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", MODE="0666"' \
  | sudo tee /etc/udev/rules.d/99-stlink.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

## Wire the ST-Link to the hoverboard
- SWDIO should be from straight line cute side of pcb and swck should be on on cuve cut side
1. SWDIO
2. GND
3. SWCLK

## Build and flash the hoverboard
- HOLD Power Button and Keep it pressed till upload is done
```bash
cd software/firmwares/hoverboard_stm32
pio run
```
- HOLD Power Button and Keep it pressed till upload is done
```bash
pio run -e VARIANT_USART -t clean
pio run -e VARIANT_USART -t upload
```

NOTE : Next to test you need to send speed commands through microcontroller code
