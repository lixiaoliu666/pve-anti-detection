# Li Xiaoliu & DadaShuai666 Project

**Authors:** Li Xiaoliu & DadaShuai666

Chat QQ Groups `25438194` with entry codes `666`

- **Li Xiaolius Bilibili:** [https://space.bilibili.com/565938745](https://space.bilibili.com/565938745)
- **DadaShuai666s Bilibili:** [https://space.bilibili.com/14205798](https://space.bilibili.com/14205798)

## Overview

I initially wanted to add the "Li Xiaoliu & DadaShuai666 Project LOGO.jpg" icon to the PVE virtual machine boot screen. Since PVEs KVM and OVMF are separate packages, compiling the OVMF package is necessary to achieve this. Now, I have successfully implemented it through the OVMF package.

For reference, you can check out my previous article: [https://www.bilibili.com/read/cv26245305](https://www.bilibili.com/read/cv26245305)

## 1. Preparation

1. **Modify MAC Address Prefix**\
   Navigate to **PVE Web UI** → **Datacenter** → **Options** → **Change MAC Address Prefix** to `D8:FC:93`

2. **Hardware Recommendations**

   - Pass through SATA or M.2 SSD to the VM.
   - Use a wired network adapter (USB network adapter for testing).

🚨 **No restrictions on SCSI or Virtio devices in this project!** 🚨

3. **Source Code and Tutorials**\
   The complete source code and tutorial series are available in my Bilibili column:\
   [PVE Anti-Virtualization Detection for Gaming](https://www.bilibili.com/read/readlist/rl758108)

## 2. Installation

### Required Packages

Download the following packages:

- `pve-qemu-kvm_9.xxx_amd64.deb` (Replace `xxx` with the specific version you downloaded)
- `pve-edk2-firmware-ovmf_xxx.deb` you can also download from https://github.com/lixiaoliu666/pve-anti-detection-edk2-firmware-ovmf

Upload them to `/root` using **WinSCP**.

If you need advanced kernel compilation, refer to my article on compiling and loading PVE Linux kernel modules:\
[https://www.bilibili.com/read/cv37772880/](https://www.bilibili.com/read/cv37772880/)

### Check Installed KVM Version

```bash
 dpkg -l | grep pve-qemu-kvm
```

### Install Anti-Detection Packages

For version **9.x**, install with:

```bash
dpkg -i pve-qemu-kvm_9.xxx_amd64.deb
dpkg -i pve-edk2-firmware-ovmf_xxx.deb
```

If your QEMU version is outdated, update your system and install the latest version:

```bash
apt update
apt install pve-qemu-kvm
dpkg -i pve-qemu-kvm_9.xxx_amd64.deb
dpkg -i pve-edk2-firmware-ovmf_xxx.deb
```

### Reboot After Installation

```bash
reboot
```

### Restore Official Packages (If Needed)

```bash
apt reinstall pve-qemu-kvm
apt reinstall pve-edk2-firmware-ovmf
```

## 3. Creating a Virtual Machine

### Recommended Configuration

- **BIOS:** OVMF + Q35 (Recommended) or OVMF + i440fx
- **Storage:** SATA (at least 128GB; avoid SCSI and Virtio devices)
- **CD Drive:** IDE or SATA
- **Graphics:** Standard initially, then pass through dedicated GPU
- **CPU:** `host` (1 socket, multiple cores)
- **Network Adapter:** `e1000` (Ensure correct MAC address to avoid detection)
- **Memory:** Use 8192MB (8GB), 16384MB (16GB), or 4096MB (4GB) only

### VM Configuration Example (`/etc/pve/qemu-server/100.conf`)

```ini
args: -cpu host,hypervisor=off,vmware-cpuid-freq=false,enforce=false,host-phys-bits=true -smbios type=0 -smbios type=9 -smbios type=8 -smbios type=8
balloon: 0
bios: ovmf
boot: order=ide2;sata0;net0
cores: 8
cpu: host
efidisk0: local:102/vm-102-disk-0.raw,efitype=4m,size=528K
ide2: none,media=cdrom
localtime: 1
memory: 16384
meta: creation-qemu=9.0.2,ctime=1724320553
name: win10
net0: e1000=D8:FC:93:56:1D:C7,bridge=vmbr0,firewall=1
numa: 0
ostype: l26
sata0: local:102/vm-102-disk-1.raw,size=128G,ssd=1
scsihw: virtio-scsi-single
smbios1: uuid=505429c8-a350-41e9-9154-3851c095254e
sockets: 1
usb0: host=0000:3825
usb1: host=258a:002a
vmgenid: 2271babc-cafc-4c68-be8b-2bb3157c9924
```

### QEMU 7 & 8 Configuration

For **QEMU 7 & 8**, use the following `args`:

```ini
args: -cpu host,hypervisor=off,vmware-cpuid-freq=false,enforce=false,host-phys-bits=true -smbios type=0,vendor="American Megatrends International LLC.",version=H3.7G,date='02/21/2023',release=3.7 -smbios type=1,manufacturer="Maxsun",product="MS-Terminator B760M",version="VER:H3.7G(2022/11/29)",serial="Default string",sku="Default string",family="Default string" -smbios type=17,loc_pfx="Controller0-ChannelA-DIMM",manufacturer="KINGSTON",speed=3200,serial=DF1EC466,part="SED3200U1888S",bank="BANK 0",asset="9876543210" -smbios type=4,sock_pfx="LGA1700",manufacturer="Intel(R) Corporation",version="12th Gen Intel(R) Core(TM) i7-12700",max-speed=4900,current-speed=3800,serial="To Be Filled By O.E.M.",asset="To Be Filled By O.E.M.",part="To Be Filled By O.E.M."
```

## 4. Additional Notes

- **For detection bypassing tools in the tools directory** 
  - **Recommended Detection Tools:**
  - **Something:** `Currently Undetected Checks.txt` 
  - **Basic:** [Virtual Machine Detection Tool]
  - **Advanced:** [`pafish64.exe`](#) and [`al-khaser`](#) are the gold standards for VM detection.

## 5. Further Reading

This project is open-source. All content outside of proprietary parts will be fully disclosed in the series:\
🔗 [PVE Anti-Virtualization Detection for Gaming](https://www.bilibili.com/read/readlist/rl758108)

We welcome forks and further experimentation!
## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=lixiaoliu666/pve-anti-detection&type=Date)](https://www.star-history.com/#lixiaoliu666/pve-anti-detection&Date)
