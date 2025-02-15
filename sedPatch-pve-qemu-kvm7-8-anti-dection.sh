#!/bin/bash
#适用于给pve-qemu-kvm9里面的qemu打补丁使用，只支持9版本(不支持kvm7和kvm8，再高没有测试)，直接放本脚本在qemu目录下，在make包之前在qemu目录运行一次本脚本就是，运行后你可以继续使用git工具生成qemu具体版本的patch文件
#参考开源项目 https://github.com/zhaodice/proxmox-ve-anti-detection 编写，处理重复劳作
#作者 李晓流 & 大大帅666 20240824出品 https://space.bilibili.com/565938745
echo "开始sed工作"
sed -i 's/QEMU v" QEMU_VERSION/ASUS v" QEMU_VERSION/g' block/vhdx.c
sed -i 's/QEMU VVFAT", 10/ASUS VVFAT", 10/g' block/vvfat.c
sed -i 's/QEMU Microsoft Mouse/ASUS Microsoft Mouse/g' chardev/msmouse.c
sed -i 's/QEMU Wacom Pen Tablet/ASUS Wacom Pen Tablet/g' chardev/wctablet.c
sed -i 's/QEMU vhost-user-gpu/ASUS vhost-user-gpu/g' contrib/vhost-user-gpu/vhost-user-gpu.c
sed -i 's/desc->oem_id/ACPI_BUILD_APPNAME6/g' hw/acpi/aml-build.c
sed -i 's/desc->oem_table_id/ACPI_BUILD_APPNAME8/g' hw/acpi/aml-build.c
sed -i 's/array, ACPI_BUILD_APPNAME8/array, "PTL "/g' hw/acpi/aml-build.c

grep "do this once" hw/acpi/vmgenid.c >/dev/null
if [ $? -eq 0 ]; then
	echo "hw/acpi/vmgenid.c 文件只能处理一次！以前已经处理，本次不执行！"
else
	sed -i 's/    Aml \*ssdt/       \/\/FUCK YOU~~~\n       return;\/\/do this once\n    Aml \*ssdt/g' hw/acpi/vmgenid.c
	echo "hw/acpi/vmgenid.c 文件处理完成（第一次处理，只处理一次）"
fi

sed -i 's/QEMU N800/ASUS N800/g' hw/arm/nseries.c
sed -i 's/QEMU LCD panel/ASUS LCD panel/g' hw/arm/nseries.c
sed -i 's/strcpy((void *) w, "QEMU ")/strcpy((void *) w, "ASUS ")/g' hw/arm/nseries.c
sed -i 's/"1.1.10-qemu" : "1.1.6-qemu"/"1.1.10-asus" : "1.1.6-asus"/g' hw/arm/nseries.c
sed -i "s/QEMU 'SBSA Reference' ARM Virtual Machine/ASUS 'SBSA Reference' ARM Real Machine/g" hw/arm/sbsa-ref.c
sed -i 's/QEMU Sun Mouse/ASUS Sun Mouse/g' hw/char/escc.c
sed -i 's/info->vendor = "RHT"/info->vendor = "DEL"/g' hw/display/edid-generate.c
sed -i 's/QEMU Monitor/DELL Monitor/g' hw/display/edid-generate.c
sed -i 's/uint16_t model_nr = 0x1234;/uint16_t model_nr = 0xA05F;/g' hw/display/edid-generate.c

grep "do this once" hw/i386/acpi-build.c >/dev/null
if [ $? -eq 0 ]; then
	echo "hw/i386/acpi-build.c 文件只能处理一次！以前已经处理，本次不执行！"
else
	sed -i '/static void build_dbg_aml(Aml \*table)/,/ /s/{/{\n     return;\/\/do this once/g' hw/i386/acpi-build.c
	sed -i '/create fw_cfg node/,/}/s/}/}*\//g' hw/i386/acpi-build.c
	sed -i '/create fw_cfg node/,/}/s/{/\/*{/g' hw/i386/acpi-build.c
	echo "hw/i386/acpi-build.c 文件处理完成（第一次处理，只处理一次）"
fi

sed -i 's/"QEMU/"ASUS/g' hw/i386/pc_piix.c
sed -i 's/Standard PC (i440FX + PIIX, 1996)/ASUS M4A88TD-Mi440fx/g' hw/i386/pc_piix.c
sed -i 's/"QEMU/"ASUS/g' hw/i386/pc_q35.c
sed -i 's/Standard PC (Q35 + ICH9, 2009)/ASUS M4A88TD-Mq35/g' hw/i386/pc_q35.c
sed -i 's/mc->name, pcmc->smbios_legacy_mode,/"ASUS-PC", pcmc->smbios_legacy_mode,/g' hw/i386/pc_q35.c
sed -i 's/"QEMU/"ASUS/g' hw/ide/atapi.c
sed -i 's/"QEMU/"ASUS /g' hw/ide/core.c
sed -i 's/QM%05d/ASUS%05d/g' hw/ide/core.c
sed -i 's/"QEMU/"ASUS/g' hw/input/adb-kbd.c
sed -i 's/"QEMU/"ASUS/g' hw/input/adb-mouse.c
sed -i 's/"QEMU/"ASUS/g' hw/input/ads7846.c
sed -i 's/"QEMU/"ASUS/g' hw/input/hid.c
sed -i 's/"QEMU/"ASUS/g' hw/input/ps2.c
sed -i 's/"QEMU/"ASUS/g' hw/input/tsc2005.c
sed -i 's/"QEMU/"ASUS/g' hw/input/tsc210x.c
sed -i 's/"QEMU Virtio/"ASUS/g' hw/input/virtio-input-hid.c
sed -i 's/QEMU M68K Virtual Machine/ASUS M68K Real Machine/g' hw/m68k/virt.c
sed -i 's/"QEMU/"ASUS/g' hw/misc/pvpanic-isa.c
sed -i 's/"QEMU/"ASUS/g' hw/nvme/ctrl.c
sed -i 's/0x51454d5520434647ULL/0x4155535520434647ULL/g' hw/nvram/fw_cfg.c
sed -i 's/"QEMU/"ASUS/g' hw/pci-host/gpex.c
sed -i 's/"QEMU/"ASUS/g' hw/ppc/e500plat.c
sed -i 's/qemu-e500/asus-e500/g' hw/ppc/e500plat.c
sed -i 's/s16s8s16s16s16/s11s4s51s41s91/g' hw/scsi/mptconfig.c
sed -i 's/QEMU MPT Fusion/ASUS MPT Fusion/g' hw/scsi/mptconfig.c
sed -i 's/"QEMU"/"ASUS"/g' hw/scsi/mptconfig.c
sed -i 's/0000111122223333/1145141919810000/g' hw/scsi/mptconfig.c
sed -i 's/"QEMU/"ASUS/g' hw/scsi/scsi-bus.c
sed -i 's/"QEMU/"ASUS/g' hw/scsi/scsi-disk.c
sed -i 's/"QEMU/"ASUS/g' hw/scsi/spapr_vscsi.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-audio.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-hid.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-hub.c
sed -i 's/314159/114514/g' hw/usb/dev-hub.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-mtp.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-network.c
sed -i 's/"RNDIS\/QEMU/"RNDIS\/ASUS/g' hw/usb/dev-network.c
sed -i 's/400102030405/400114514405/g' hw/usb/dev-network.c
sed -i 's/s->vendorid = 0x1234/s->vendorid = 0x8086/g' hw/usb/dev-network.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-serial.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-smartcard-reader.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-storage.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-uas.c
sed -i 's/27842/33121/g' hw/usb/dev-uas.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/dev-wacom.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/u2f-emulated.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/u2f-passthru.c
sed -i 's/"QEMU/"ASUS/g' hw/usb/u2f.c
sed -i 's/"BOCHS/"INTEL/g' include/hw/acpi/aml-build.h
sed -i 's/"BXPC/"PC8086/g' include/hw/acpi/aml-build.h
sed -i 's/"QEMU0002/"ASUS0002/g' include/standard-headers/linux/qemu_fw_cfg.h
sed -i 's/0x51454d5520434647ULL/0x4155535520434647ULL/g' include/standard-headers/linux/qemu_fw_cfg.h
sed -i 's/"QEMU/"ASUS/g' migration/migration.c
sed -i 's/"QEMU/"ASUS/g' migration/rdma.c
sed -i 's/0x51454d5520434647ULL/0x4155535520434647ULL/g' pc-bios/optionrom/optionrom.h
sed -i 's/"QEMU/"ASUS/g' pc-bios/s390-ccw/virtio-scsi.h
sed -i 's/"QEMU/"ASUS/g' roms/seabios/src/fw/ssdt-misc.dsl
sed -i 's/"QEMU/"ASUS/g' roms/seabios-hppa/src/fw/ssdt-misc.dsl
sed -i 's/KVMKVMKVM\\0\\0\\0/GenuineIntel/g' target/i386/kvm/kvm.c
sed -i 's/QEMUQEMUQEMUQEMU/ASUSASUSASUSASUS/g' target/s390x/tcg/misc_helper.c
sed -i 's/"QEMU/"ASUS/g' target/s390x/tcg/misc_helper.c
sed -i 's/"KVM/"ATX/g' target/s390x/tcg/misc_helper.c
sed -i 's/t->bios_characteristics_extension_bytes\[1\] = 0x14;/t->bios_characteristics_extension_bytes\[1\] = 0x0F;/g' hw/smbios/smbios.c
sed -i 's/t->voltage = 0;/t->voltage = 0x8B;/g' hw/smbios/smbios.c
sed -i 's/t->external_clock = cpu_to_le16(0);/t->external_clock = cpu_to_le16(100);/g' hw/smbios/smbios.c
sed -i 's/t->l1_cache_handle = cpu_to_le16(0xFFFF);/t->l1_cache_handle = cpu_to_le16(0x0039);/g' hw/smbios/smbios.c
sed -i 's/t->l2_cache_handle = cpu_to_le16(0xFFFF);/t->l2_cache_handle = cpu_to_le16(0x003A);/g' hw/smbios/smbios.c
sed -i 's/t->l3_cache_handle = cpu_to_le16(0xFFFF);/t->l3_cache_handle = cpu_to_le16(0x003B);/g' hw/smbios/smbios.c
sed -i 's/t->processor_family = 0x01;/t->processor_family = 0xC6;/g' hw/smbios/smbios.c
sed -i 's/t->processor_characteristics = cpu_to_le16(0x02);/t->processor_characteristics = cpu_to_le16(0x04);/g' hw/smbios/smbios.c
sed -i 's/t->memory_type = 0x07;/t->memory_type = 0x1A;/g' hw/smbios/smbios.c
sed -i 's/t->total_width = cpu_to_le16(0xFFFF);/t->total_width = cpu_to_le16(64);/g' hw/smbios/smbios.c
sed -i 's/t->data_width = cpu_to_le16(0xFFFF);/t->data_width = cpu_to_le16(64);/g' hw/smbios/smbios.c
sed -i 's/t->minimum_voltage = cpu_to_le16(0);/t->minimum_voltage = cpu_to_le16(1200);/g' hw/smbios/smbios.c
sed -i 's/t->maximum_voltage = cpu_to_le16(0);/t->maximum_voltage = cpu_to_le16(1350);/g' hw/smbios/smbios.c
sed -i 's/t->configured_voltage = cpu_to_le16(0);/t->configured_voltage = cpu_to_le16(1200);/g' hw/smbios/smbios.c
sed -i 's/t->location = 0x01;/t->location = 0x03;/g' hw/smbios/smbios.c
sed -i 's/t->error_correction = 0x06;/t->error_correction = 0x03;/g' hw/smbios/smbios.c
sed -i 's/"QEMU TCG CPU version/"TCG CPU version/g' target/i386/cpu.c
sed -i 's/"Microsoft Hv/"GenuineIntel/g' target/i386/cpu.c  #解决n卡vgpu驱动43问题
echo "结束sed工作"
