#!/sbin/sh
# Usage Note
# This script has no device model verification,
# do not flash and then roll back to older system versions
# Adapted from CoolAPK user @淡存（uid:1763518）
export OUTFD=$4;
package=$1


show_progress() { echo "progress $1 $2" >> /proc/self/fd/$OUTFD; }
ui_print() { until [ -z "$1" ]; do echo "ui_print $1\nui_print" >> /proc/self/fd/$OUTFD; shift; done; }
abort() { ui_print "$@"; exit 1; }


grep_cmdline() {
  local REGEX="s/^$1=//p"
  { echo $(cat /proc/cmdline) | xargs -n 1; sed -e 's/ = /=/g' /proc/bootconfig; } 2>/dev/null | sed -n "$REGEX"
}
get_slot() {
  local SLOT=$(grep_cmdline androidboot.slot_suffix)
  [ -z "$SLOT" ] && SLOT=$(grep_cmdline androidboot.slot) && [ -n "$SLOT" ] && SLOT=_${SLOT}
  [ "$SLOT" != "normal" ] && echo "$SLOT"
}


slot=$(get_slot)
slot=$(echo "$slot" | tr -d '"' | tr -d ' ') 
if [ "$slot" = "_a" ]; then
    target_slot="_b"
    target_index=1
else 
    target_slot="_a"
    target_index=0
fi


# OnePlus 15
# supersize=18907922432
# groupsize=18903728128

metadatasize=65536

supersize=$(blockdev --getsize64 /dev/block/by-name/super)
groupsize=$(echo "$supersize - 4194304" | bc)

super_group=qti_dynamic_paritions
tmpdir=/data/tmp
qti_group=${super_group}${target_slot}


mksuper(){
  Imgdir=$1
  outputimg=$2
  
  super="--metadata-size $metadatasize --super-name super --virtual-ab --block-size 4096 "
  super+="--device-size $supersize --metadata-slots 3 "
  super+="--group ${qti_group}:$groupsize --group ${super_group}${slot}:$groupsize " 
  
  for imag in $(basename -a "$Imgdir"/*.img);do
    image=${imag%.img} 
    img_path="$Imgdir/$image.img"
    [ ! -s "$img_path" ] && continue 
    
    img_size=$(wc -c < "$img_path")
    super+="--partition ${image}${target_slot}:readonly:${img_size}:${qti_group} --image ${image}${target_slot}=${img_path} "
    super+="--partition ${image}${slot}:readonly:0:${super_group}${slot} "
  done  
  
  super+="--force-full-image --output $outputimg"
  if ! lpmake $super 2>/tmp/lpmake_err.log; then
    abort "-- lpmake failed: $(cat /tmp/lpmake_err.log)"
  fi
  ui_print "-- super.img synthesized successfully"
}

flashImg(){
  Imgdir=$1 
  target_imgs="vbmeta_system.img boot.img init_boot.img vendor_boot.img recovery.img dtbo.img vbmeta.img vbmeta_vendor.img xbl.img xbl_config.img engineering_cdt.img uefi.img aop.img aop_config.img tz.img hyp.img modem.img bluetooth.img abl.img dsp.img keymaster.img spuservice.img devcfg.img qupfw.img uefisecapp.img imagefv.img shrm.img cpucp.img featenabler.img oplus_sec.img splash.img xbl_ramdump.img cpucp_dtb.img soccp_debug.img socpp_dcd.img pdp.img pdp_cdb.img tme_seq_patch.img hyp_ac_config.img multiimgoem.img tme_fw.img soccp.img multiimgqti.img tme_config.img xbl_ac_config.img secretkeeper.img tz_ac_config.img dcp.img pvmfw.img"
  
  for img_name in $target_imgs; do
    img_path="$Imgdir/$img_name"
    part_name=${img_name%.img}
    [ ! -s "$img_path" ] && { ui_print "-- Skip missing $img_name"; continue; }
    
    target_part="${part_name}${target_slot}"
    ui_print "-- Flashing $target_part"
    dd if="$img_path" of="/dev/block/by-name/${target_part}" bs=4M || abort "-- Failed to flash $target_part"
    ui_print "---- $part_name completed ----"
  done
}


ui_print "          FlashTool         "
ui_print "-- Current Slot: $slot  Flash Slot: $target_slot"
show_progress 0.1 10;

rm -rf $tmpdir && mkdir -p $tmpdir
ui_print "-- Extracting OTA package..."
startTime_s=$(date +%s)
payload_extract -i "$package" -x -o "$tmpdir/payload" -T4
ui_print "-- Extraction Time: $(( $(date +%s) - startTime_s )) seconds"


ui_print "-- Using preset OxygenOS my_company and my_preload"
cp -f "/system/bin/my_company.img" "$tmpdir/payload/"
cp -f "/system/bin/my_preload.img" "$tmpdir/payload/"


ui_print "-- Synthesizing super.img..."
startTime_s=$(date +%s)
mkdir -p $tmpdir/super
for img in system.img system_ext.img product.img vendor.img vendor_dlkm.img system_dlkm.img odm.img my_product.img my_engineering.img my_stock.img my_heytap.img my_carrier.img my_region.img my_bigball.img my_manifest.img my_preload.img my_company.img; do
  [ -f "$tmpdir/payload/$img" ] && mv -f "$tmpdir/payload/$img" "$tmpdir/super"
done
mksuper $tmpdir/super $tmpdir/super.img  
ui_print "-- Synthesis Time: $(( $(date +%s) - startTime_s )) seconds"
[ ! -s "$tmpdir/super.img" ] && abort "super.img synthesis failed"

ui_print "-- Flashing images..."
startTime_s=$(date +%s)
flashImg $tmpdir/payload
ui_print "-- Flashing Time: $(( $(date +%s) - startTime_s )) seconds"

ui_print "-- Flashing super.img..."
cat "$tmpdir/super.img" > "/dev/block/by-name/super"  || abort "Failed to flash super.img"

ui_print "-- Switching boot slot to $target_slot"
bootctl set-active-boot-slot $target_index || ui_print "!!! Slot switch failed, manually run: fastboot set_active ${target_slot/_/}"
avbctl disable-verity --force && avbctl disable-verification --force
rm -rf /tmp $tmpdir
ui_print "--- Installation completed, mount failure prompts can be ignored ---"
show_progress 0.1 10;

