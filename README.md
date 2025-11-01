# OnePlus 15 infiniti OrangeFox device tree


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
git clone https://github.com/koaaN/android_device_infiniti-orangefox infiniti
```
### BUILD!
```
cd ~/fox_14.1
source build/envsetup.sh
lunch twrp_infiniti-ap2a-eng
mka adbd recoveryimage
```
