# 安装Wine 9的教程

本教程用于安装能跑差分工具的wine版本, 当前验证过的版本是wine9 , 操作系统ubuntu 22.04 和 centos 7

## CentOS 7下的安装教程

建立目录, 下载安装文件

```bash
mkdir /opt/wine
cd /opt/wine

# 下载源码包
wget https://dl.winehq.org/wine/source/9.0/wine-9.0.tar.xz
# 或者
wget http://43.129.160.151/wine/source/9.0/wine-9.0.tar.xz

tar xf wine-9.0.tar.xz
```

安装依赖包

```bash
yum install gcc g++ flex bison make freetype-devel freetype-devel.i686
yum install gcc-multilib g++-multilib
yum install libgcc.i686 glibc-devel.i686 libstdc++-devel.i686
```

配置并编译

```bash
./configure --without-x --without-freetype --enable-win64
make -j16
```

安装到 /usr/local

```bash
make install
```

验证执行情况

```bash
cd luatos-soc-2022/tools/dtools
# 准备2个binpkg文件 old.binpkg new.binpkg
# 然后执行
wine ./FotaToolkit.exe -d config/ec618.json BINPKG diff.par old.binpkg new.binpkg
```

## Ubuntu 22.04下的安装教程

与centos不同, ubuntu可以直接用镜像源安装, 速度更快, 不需要编译源码

```bash
sed -i 's/security.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/g' /etc/apt/sources.list
sed -i 's/archive.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/g' /etc/apt/sources.list
apt-get update -y && apt install -y wget python3-pip
wget -nc -O /usr/share/keyrings/winehq-archive.key https://dl.winehq.org/wine-builds/winehq.key
# COPY winehq-archive.key /usr/share/keyrings/winehq-archive.key
echo "deb [arch=amd64,i386 signed-by=/usr/share/keyrings/winehq-archive.key] https://mirrors.tuna.tsinghua.edu.cn/wine-builds/ubuntu/ jammy main" > /etc/apt/sources.list.d/winehq.list
dpkg --add-architecture i386 &&\
    apt-get update &&\
    apt-get install --install-recommends -y winehq-stable &&\
    apt clean -y
```
