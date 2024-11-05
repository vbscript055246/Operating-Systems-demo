#
# Install build kernel tools
#
sudo apt update
sudo apt-get install build-essential libncurses5-dev git exuberant-ctags bc libssl-dev bison flex libelf-dev zstd -y
sudo apt upgrade -y && sudo apt autoremove -y

#
# Download and unzip the kernel source
#
wget https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.19.12.tar.xz
tar -xvf linux-5.19.12.tar.xz

#
# Create working directory
#
cd linux-5.19.12 && mkdir mysyscall && cd mysyscall

#
# Write new syscall function
#
base64 -d <<< "I2luY2x1ZGUgPGxpbnV4L3N5c2NhbGxzLmg+CiNpbmNsdWRlIDxsaW51eC9rZXJuZWwuaD4KClNZU0NBTExfREVGSU5FKGhlbGxvKQp7CiAgICBwcmludGsoIkhlbGxvIHdvcmxkXG4iKTsKICAgIHByaW50aygiMzEwNTU1MDA3XG4iKTsKICAgIHJldHVybiAwOwp9" > hello.c
base64 -d <<< "I2luY2x1ZGUgPGxpbnV4L3N5c2NhbGxzLmg+CiNpbmNsdWRlIDxsaW51eC91YWNjZXNzLmg+CiNpbmNsdWRlIDxsaW51eC9rZXJuZWwuaD4KCgpTWVNDQUxMX0RFRklORTIocmV2c3RyLCB1bnNpZ25lZCBpbnQsIGxlbiwgY29uc3QgY2hhciBfX3VzZXIgKiwgc3RyKQp7CgogICAgaW50IGk9MDsKICAgIGNoYXIgYnVmWzI1Nl07CiAgICAKICAgIGlmKGNvcHlfZnJvbV91c2VyKGJ1Ziwgc3RyLCBsZW4pICE9IDAgKSByZXR1cm4gLUVGQVVMVDsKCiAgICBidWZbbGVuXSA9ICdcMCc7CiAgICBwcmludGsoIlRoZSBvcmlnaW4gc3RyaW5nOiAlc1xuIiwgYnVmKTsKICAgIAogICAgZm9yKGk9MDtpPGxlbi8yO2krKyl7CiAgICAgICAgYnVmW2ldIF49IGJ1ZltsZW4tMS1pXTsKICAgICAgICBidWZbbGVuLTEtaV0gXj0gYnVmW2ldOwogICAgICAgIGJ1ZltpXSBePSBidWZbbGVuLTEtaV07CgogICAgfSAgCiAgICBwcmludGsoIlRoZSByZXZlcnNlZCBzdHJpbmc6ICVzXG4iLCBidWYpOwogICAgCiAgICByZXR1cm4gMDsKfQ==" > revstr.c

#
# Add hello.o and revstr.o to the obj target 
#
echo '
obj-y := hello.o
obj-y += revstr.o
' > Makefile
cd ..


# include/linux/syscalls.h
# 
# Add new system call to the system call header file
# 
sed -i '312 i asmlinkage long sys_hello(void);' include/linux/syscalls.h
sed -i '313 i asmlinkage long sys_revstr(unsigned int len, const char __user *str);' include/linux/syscalls.h

# arch/x86/entry/syscalls/syscall_64.tbl
# 
# Add new system call to the system call entry
# 
sed -i '$ a 466 64 hello sys_hello' arch/x86/entry/syscalls/syscall_64.tbl
sed -i '$ a 467 64 revstr sys_revstr' arch/x86/entry/syscalls/syscall_64.tbl

# core-y add mysyscall/
# 
# Add new system call directory to the target
# 
sed -i '1103s:$: mysyscall/:' Makefile

#
# Get current kernel config
#
cp /boot/config-`uname -r`* .config && make menuconfig

# Alter .config
## CONFIG_LOCALVERSION 
sed -i 's:CONFIG_LOCALVERSION="":CONFIG_LOCALVERSION=\"-os-310555007\"' .config 

## CONFIG_SYSTEM_TRUSTED_KEYS
sed -i 's:"debian/canonical-certs.pem":"":' .config

## CONFIG_SYSTEM_REVOCATION_KEYS
sed -i 's:"debian/canonical-revoked-certs.pem":"":' .config

## CONFIG_DEBUG_INFO_BTF
sed -i 's:CONFIG_DEBUG_INFO_BTF=y:# CONFIG_DEBUG_INFO_BTF is not set:' .config

#
# Build kernel & Install
#
time make -j9

sudo make -j9 modules_install install

sudo reboot
