# OnePlus 15 infiniti OrangeFox device tree

## Working

- [X] Display
- [X] Touch 
- [X] Decryption
- [X] Flashing
- [X] Backup & Restore
- [X] MTP/OTG Storage
- [X] ADB/FastbootD
- [X] Factory Reset
- [X] Vibrator
- [X] Display & Vibration Settings

## Not working
- [ ] Flashlight
- [ ] Root-installer (6.12 kernel not supported for now)

# How To Build

### Clone & Sync Source
```
mkdir -p ~/OrangeFox_14
cd ~/OrangeFox_14
git clone https://gitlab.com/OrangeFox/sync.git
cd sync
./orangefox_sync.sh --branch 14.1 --path ~/android/fox_14.1
```
### Clone Device-tree
```
cd ~/android/fox_14.1/device
mkdir -p oneplus
cd oneplus
git clone https://github.com/koaaN/android_device_infiniti-orangefox infiniti
```
### BUILD!
```
cd ~/android/fox_14.1
source build/envsetup.sh
lunch twrp_infiniti-ap2a-eng
mka adbd recoveryimage
```
