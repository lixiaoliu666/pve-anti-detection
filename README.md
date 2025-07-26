Language [<a href="README.md">中文</a>] | [<a href="README.en.MD">English</a>] 

李晓流 & 大大帅666 作品

qq群聊天 25438194 进群暗号 666

李晓流 b站地址：https://space.bilibili.com/565938745

大大帅666 b站地址 https://space.bilibili.com/14205798

20250725更新:实现了无序三件套效果（只需要重启一下虚拟机就自动变化）：内存序列号随机，ide和sata硬盘序列号和固件号随机，主板型号随机。

本想把 李晓流 & 大大帅666 作品 LOGO.jpg 这个图标搞进pve虚拟机启动画面的，pve的kvm和ovmf分别是两个包不是同一个包，要编译ovmf包才能实现开机画面，现在通过ovmf包实现了。

可以参考以前我发布的这个文章：https://www.bilibili.com/read/cv26245305 该文章抛砖引玉而已

1、前期准备工作：

pve网页 数据中心-》选项-》MAC地址前缀你先改成D8:FC:93

我个人的建议是直通sata硬盘或者m2硬盘给虚拟机，还有有线物理网卡（usb网卡）给虚拟机进行测试。

！！！！！！！！！！！本项目都不对scsi以及virtio设备有使用限制！！！！！！！！！！！！！！！！！！！！！！！！！！！！

我也在b站这个专栏发布全部的源码和教程如何编译源码，是整个系列的发布： https://www.bilibili.com/read/readlist/rl758108 pve反虚拟化检测玩游戏

2、正式开始

请把2个包

pve-qemu-kvm_9.xxx_amd64.deb  本项目下载  xxx代表你具体下载的版本

pve-edk2-firmware-ovmf_xxx.deb 本项目下载 ，也可以这个项目进行下载 https://github.com/lixiaoliu666/pve-anti-detection-edk2-firmware-ovmf

请用winscp 上传到/root目录下

如果你要高级的比如内核编译加东西，参考关于内核模块怎么单独编译并立马使用生效请参考我的这篇文章 https://www.bilibili.com/read/cv37772880/ 单独编译pve linux内核模块（比如kvm）加载到当前内核中立马使用生效

3、查询目前安装的kvm包版本命令

dpkg -l|grep pve-qemu-kvm

4、如果是9.x，直接安装这2个反检测包就是

dpkg -i pve-qemu-kvm_9.xxx_amd64.deb  xxx代表你具体下载的版本

dpkg -i pve-edk2-firmware-xxx.deb


如果qemu版本不是最新的，你升级下系统并安装本项目下载的最新包，命令如下:

apt update

apt install pve-qemu-kvm

dpkg -i pve-qemu-kvm_9.xxx_amd64.deb xxx代表你具体下载的版本

dpkg -i pve-edk2-firmware-ovmf_xxx.deb


安装完成后请reboot机器

reboot

如果你要恢复官方包只需要运行下面两个命令就能恢复官方包

apt reinstall pve-qemu-kvm

#如果reinstall失败或者不成功可以执行这个命令强制重装指定版本  apt install pve-qemu-kvm=9.0.2-2 或者 apt reinstall pve-qemu-kvm=9.0.2-2

apt reinstall pve-edk2-firmware-ovmf

5、新建虚拟机

虚拟机使用ovmf+q35（推荐q35）或者ovmf+i440fx，配置中注意硬盘一定选择sata硬盘（至少128g，50g 80g等大小太不像物理机硬盘大小，别对硬盘大小太抠抠扣扣搜搜了，scsi及virtio硬盘光驱网卡设备等避开使用），ide或者sata光驱，显示先选择标准（弄好后再直通独显核显vgpu等），cpu选择host（1插槽多核心这点一定注意），网卡选择e1000显卡（注意网卡mac地址问题，免得检测虚拟机），避开各种virtio设备（scsi硬盘scsi光驱等），并修改虚拟机的args参数和我一样。内存请使用8192 16384 4096这三个数值（更加像物理机内存大小），对应8g 16g 4g，其他大小请勿设置（太假太像虚拟机）。

只有一个原则：硬盘大小，内存大小，网卡都得像真实物理机配置！！！

使用以下类似命令修改虚拟机配置

nano /etc/pve/qemu-server/100.conf

我的完整虚拟机配置如下(适用于qemu 9版本，qemu 7和qemu 8请看补充):

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

补充：qemu 9 虚拟机中的args参数我现在全部做了隐藏内部定制了，不需要指定了，就用我这上面的args就是。aida64你进去看就知道有些什么硬件了（风扇、温度、电压等）

如果是qemu 7和8，需要使用下面的args

args: -cpu host,hypervisor=off,vmware-cpuid-freq=false,enforce=false,host-phys-bits=true -smbios type=0,vendor="American Megatrends International LLC.",version=H3.7G,date='02/21/2023',release=3.7 -smbios type=1,manufacturer="Maxsun",product="MS-Terminator B760M",version="VER:H3.7G(2022/11/29)",serial="Default string",sku="Default string",family="Default string" -smbios type=2,manufacturer="Maxsun",product="MS-Terminator B760M",version="VER:H3.7G(2022/11/29)",serial="Default string",asset="Default string",location="Default string" -smbios type=3,manufacturer="Default string",version="Default string",serial="Default string",asset="Default string",sku="Default string" -smbios type=17,loc_pfx="Controller0-ChannelA-DIMM",manufacturer="KINGSTON",speed=3200,serial=DF1EC466,part="SED3200U1888S",bank="BANK 0",asset="9876543210" -smbios type=4,sock_pfx="LGA1700",manufacturer="Intel(R) Corporation",version="12th Gen Intel(R) Core(TM) i7-12700",max-speed=4900,current-speed=3800,serial="To Be Filled By O.E.M.",asset="To Be Filled By O.E.M.",part="To Be Filled By O.E.M." -smbios type=8,internal_reference="CPU FAN",external_reference="Not Specified",connector_type=0xFF,port_type=0xFF -smbios type=8,internal_reference="J3C1 - GMCH FAN",external_reference="Not Specified",connector_type=0xFF,port_type=0xFF -smbios type=8,internal_reference="J2F1 - LAI FAN",external_reference="Not Specified",connector_type=0xFF,port_type=0xFF -smbios type=11,value="Default string"

6、其他内容详见本项目tools目录，里面有目前过不了的检测说明.txt，虚拟机检测工具.rar，还有高级检测软件。高级检测还得是al-khaser和pafish64.exe检测软件。pafish和al-khaser是虚拟机环境检测的两个金标准。

7、https://www.bilibili.com/read/cv26245305 

该文章中开源内容外的内容全部开源，将在这个系列全部公布: 
https://www.bilibili.com/read/readlist/rl758108 pve反虚拟化检测玩游戏

本项目抛砖引玉，欢迎fork本项目后自我继续折腾！！！
## Star History

[![Star History Chart](https://api.star-history.com/svg?repos=lixiaoliu666/pve-anti-detection&type=Date)](https://www.star-history.com/#lixiaoliu666/pve-anti-detection&Date)
