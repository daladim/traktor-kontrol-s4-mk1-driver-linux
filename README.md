## Traktor Kontrol S4 Mk1 v0.1 Driver for Linux

Linux driver based on the awesome work from blaxpot and Javi Fonseca on the Traktor Kontrol S4 Mk1 for work with Mixxx.

[traktor-s4-mk1-midify](https://github.com/blaxpot/traktor-s4-mk1-midify)

[Thread from the Mixxx forum](https://mixxx.discourse.group/t/native-instruments-kontrol-s4-v0-1/11942)

## Requirements
- **[RtMidi Library](https://www.music.mcgill.ca/~gary/rtmidi/)**
- **[ALSA Library](https://www.alsa-project.org/wiki/Main_Page)**
- **snd-usb-caiaq** Kernel Module

## Usage

As a command line driver, you can use these parameters:
- **--help** Show help information
- **--logMode** Set the log mode: console | logfile (default) currently
- **--logLevel** Set the log verbose level: info | debug currently
- **--configFile** Set the path where the config file is. Based on JSON format and included in this repo: **config.json**

## Build
1. Install dependencies. The package names will depend on your Linux distribution.

**Debian/Ubuntu/Raspberry Pi OS**
```
sudo apt install cmake cmake-extras libasound2 librtaudio-dev librtmidi-dev libevdev-dev
```

**Arch Linux**
```
sudo pacman -Syu base-devel cmake alsa-lib rtaudio rtmidi libevdev
```

2. Build with `cmake`:
```
mkdir build
cd build
cmake ..
cmake --build .
```

If all goes well, you should now be able to start the application like so:
```
# ./traktor_kontrol_s4_mk1_driver_linux --logMode console
[2023-01-20 09:51:40.493] [traktor_kontrol_s4_logger] [info] [main::init_application] Traktor Kontrol S4 Mk1 Driver for Linux started
[2023-01-20 09:51:40.493] [traktor_kontrol_s4_logger] [info] [main:show_main_configuration] Current arguments:
[2023-01-20 09:51:40.493] [traktor_kontrol_s4_logger] [info] [main:show_main_configuration] Log level: INFO
[2023-01-20 09:51:40.493] [traktor_kontrol_s4_logger] [info] [main:show_main_configuration] Log mode: LOG IN CONSOLE
[2023-01-20 09:51:40.493] [traktor_kontrol_s4_logger] [info] [main::init_application] Starting helpers....
[2023-01-20 09:51:40.503] [traktor_kontrol_s4_logger] [info] [main::init_application] Get MIDI information....
[2023-01-20 09:51:40.505] [traktor_kontrol_s4_logger] [info] [main::init_application] Initializing EvDev device....
```

## Installation
After building, the driver can be run manually from the `build` directory or managed as a systemd user service. A user service is recommended when Mixxx runs in the same desktop session because it runs with the logged-in user's ALSA and evdev access.

### systemd user service

1. Set `REPO_DIR` to the absolute path of your checkout and create the user-unit directory:

```bash
REPO_DIR="$HOME/path/to/traktor-kontrol-s4-mk1-driver-linux"
mkdir -p "$HOME/.config/systemd/user"
```

2. Create `~/.config/systemd/user/traktor-kontrol-s4.service` using the checkout path:

```bash
cat > "$HOME/.config/systemd/user/traktor-kontrol-s4.service" <<EOF
[Unit]
Description=Traktor Kontrol S4 Mk1 driver

[Service]
Type=simple
WorkingDirectory=${REPO_DIR}/build
ExecStart=${REPO_DIR}/build/traktor_kontrol_s4_mk1_driver_linux --logMode console --configFile ${REPO_DIR}/config.json
Restart=on-failure
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=default.target
EOF
```

`WorkingDirectory` preserves the normal build-directory invocation. The explicit `--configFile` path prevents the service from depending on a relative config-file path.

3. Validate, enable, and start the service:

```bash
systemd-analyze verify "$HOME/.config/systemd/user/traktor-kontrol-s4.service"
systemctl --user daemon-reload
systemctl --user enable --now traktor-kontrol-s4.service
```

4. Check its status and follow its console output through the journal:

```bash
systemctl --user status traktor-kontrol-s4.service
journalctl --user -u traktor-kontrol-s4.service -f
```

The service starts when the user session starts and stops when the user logs out. To keep it running without an active login session, enable lingering for the user:

```bash
loginctl enable-linger "$USER"
```

To stop and remove the service:

```bash
systemctl --user disable --now traktor-kontrol-s4.service
rm "$HOME/.config/systemd/user/traktor-kontrol-s4.service"
systemctl --user daemon-reload
```

## IN PROCESS
