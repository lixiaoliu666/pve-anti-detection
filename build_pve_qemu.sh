sudo apt-get update
sudo apt-get install libacl1-dev libaio-dev libattr1-dev libcap-ng-dev libcurl4-gnutls-dev libepoxy-dev libfdt-dev libgbm-dev libglusterfs-dev libgnutls28-dev libiscsi-dev libjpeg-dev libnuma-dev libpci-dev libpixman-1-dev libproxmox-backup-qemu0-dev librbd-dev libsdl1.2-dev libseccomp-dev libslirp-dev libspice-protocol-dev libspice-server-dev libsystemd-dev liburing-dev libusb-1.0-0-dev libusbredirparser-dev libvirglrenderer-dev meson python3-sphinx python3-sphinx-rtd-theme quilt xfslibs-dev
ls
df -h
git clone git://git.proxmox.com/git/pve-qemu.git
cd pve-qemu
git reset --hard dfac8d424dbdfdc7133cb4e2292d51efce104c52
apt install devscripts -y
mk-build-deps --install
make
make clean
#cp ../sedPatch-pve-qemu-kvm7-8-anti-dection.sh qemu/
cd qemu
#chmod +x sedPatch-pve-qemu-kvm7-8-anti-dection.sh
#bash sedPatch-pve-qemu-kvm7-8-anti-dection.sh
#cp ../../smbios.h include/hw/firmware/smbios.h
#cp ../../smbios.c hw/smbios/smbios.c
sed -i 's/!object_dynamic_cast/object_dynamic_cast/g' hw/vfio/igd.c
sed -i 's/return -1;/return 8;/g' hw/vfio/igd.c
cd ..
apt install devscripts -y
mk-build-deps --install
make

<<EOF
git submodule update --init
ls
cd qemu
git submodule update --init
cd ..
cp ../sedPatch-pve-qemu-kvm7-8-anti-dection.sh qemu/
cd qemu
chmod +x sedPatch-pve-qemu-kvm7-8-anti-dection.sh
bash sedPatch-pve-qemu-kvm7-8-anti-dection.sh
#cp ../../smbios.h include/hw/firmware/smbios.h
#cp ../../smbios.c hw/smbios/smbios.c
cd ..
apt install devscripts -y
mk-build-deps --install
make
EOF

<<EOF
apt install devscripts -y
mk-build-deps --install
make
make clean
cp ../sedPatch-pve-qemu-kvm7-8-anti-dection.sh qemu/
cd qemu
chmod +x sedPatch-pve-qemu-kvm7-8-anti-dection.sh
bash sedPatch-pve-qemu-kvm7-8-anti-dection.sh
cp ../../smbios.h include/hw/firmware/smbios.h
cp ../../smbios.c hw/smbios/smbios.c
cd ..
apt install devscripts -y
mk-build-deps --install
make
EOF
