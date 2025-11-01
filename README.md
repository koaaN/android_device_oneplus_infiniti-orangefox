# OnePlus 13 dodge Android device tree

## Working

- [X] Display
- [X] Touch (Even in FastbootD)
- [X] Decryption
- [X] Flashing
- [X] Backup & Restore
- [X] MTP/OTG Storage
- [X] ADB/FastbootD
- [X] Factory Reset
- [X] Vibrator
- [X] Display & Vibration Settings

## Not working
- [ ] ????????

# How To Build

### Clone & Sync Source
```
mkdir -p ~/OrangeFox_14
cd ~/OrangeFox_14
git clone https://gitlab.com/OrangeFox/sync.git
cd sync
./orangefox_sync.sh --branch 14.1 --path ~/fox_14.1
```
### Clone Device-tree
```
cd ~/fox_14.1/device
mkdir -p oneplus
cd oneplus
git clone https://gitlab.com/OrangeFox/device/dodge.git dodge
```
### BUILD!
```
cd ~/fox_14.1
source build/envsetup.sh
lunch twrp_dodge-ap2a-eng
mka adbd recoveryimage
```
