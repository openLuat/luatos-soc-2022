# CSDK for ABCDEGF

[![Github Actions](https://github.com/openLuat/luatos-soc-2022/actions/workflows/ci.yaml/badge.svg?branch=master)](https://github.com/openLuat/luatos-soc-2022/actions/workflows/ci.yaml)

**本仓库将在4月15日关闭，请谨慎决定是否仍然基于本仓库CSDK进行开发。**

**待合宙有能力将CSDK做成精品，且有足够的能力提供详尽的API和文档时，我们再考虑适时开放，但时间无法确定。**

**推荐您选择合宙LuatOS进行二次开发，合宙会举全公司之力持续的将LuatOS的API和文档做完善，进一步降低您的开发难度。**

**感谢您一直以来对合宙的信任和支持，也再次请您原谅由于我们的能力不足而将CSDK关闭的决定。**

**谢谢！**


## 使用前须知

1. **强烈推荐使用git来下载本代码库**, 不会命令行操作git可以用 [tortoiseGit](https://tortoisegit.org/download/)
2. 本CSDK底层是没有AT指令的,不存在"调用某某AT指令"的情况

## 文档中心

[CSDK软件开发资料](https://doc.openluat.com/wiki/37?wiki_page_id=4544)

## 介绍

CSDK for ABCDEGF, 适用于所有基于ABCDEGF的合宙模组, 包括衍生型号及海外型号:

* [Air780E](https://air780e.cn)   合宙主推封装
* [Air600E](https://air600e.cn)   兼容友商的封装
* [Air700E](https://air700e.cn)   极致小封装
* [Air780EG](https://air780eg.cn)  带GNSS定位
* [Air780EX](https://air780ex.cn)  邮票孔封装

## 应用文档

1. 添加自定义数据到指定区域并[合成binpkg](project/example_flash), 用于量产刷机
2. [脱离本csdk编译无依赖的库文件](doc/build_lib_by_gcc.md)
3. Linux/Mac刷机工具(测试版) [ectool2py](https://github.com/openLuat/ectool2py)
4. [差分升级所需要工具,支持docker](tools/dtools)
5. [升级包格式及生成方法](doc/upgrade_pkg.md)

## 目录说明

* PLAT 打包好的sdk, 含协议栈, 部分源码, 头文件, 打包工具
* project 项目文件, 每个不同的项目代表不同的demo或者turnkey方案
* xmake.lua 主编译脚本, 一般不需要修改

## 编译说明

1. 安装xmake, **选取加入PATH**, 建议安装到C:\Program Files下，其他目录可能会出问题；
   xmake下载地址：[xmake官网](https://xmake.io/#/guide/installation)
   windows下可以直接访问：[合宙云盘](https://pan.air32.cn/s/DJTr?path=%2F%E5%B8%B8%E7%94%A8%E5%B7%A5%E5%85%B7) ,下载系统对应的xmake-2.7.3-win32.exe或者xmake-2.7.3-win64.exe即可. 比2.7.3更高的版本也是可以的.

   **注意：环境变量需重启电脑生效**

2. 如果编译example的话在本代码库的根目录执行`build.bat example`进行编译

3. 生成的binpkg位于`out`目录, 日志数据库文件位于`PLAT`目录

## 如何新增自己的项目

建议将 `project/example` 拷贝一份,作为验证项目

1. project新建目录，目录名称和项目名称一致，目录下新建xmake.lua，内容仿照着example写，核心是TARGET_NAME必须和项目名称一致
2. 代码路径当然不是限制的，在SDK的任何目录，甚至电脑上的其他地方，前提是你会改project里xmake.lua内的源码路径
3. 根目录执行build.bat 你的项目名称

## 项目中新增入口函数

本CSDK中没有main函数,需要通过宏注册入口函数

* 加入头文件common_api.h
* 类似于`void main(void)`，但是可以任意名字，不能有入口参数和返回参数，同时允许多个入口
* 通过INIT_HW_EXPORT INIT_DRV_EXPORT INIT_TASK_EXPORT这3个宏修饰后，系统初始化的时候会调用function，不同的宏涉及到调用顺序不同
* 大优先级上`INIT_HW_EXPORT` > `INIT_DRV_EXPORT` > `INIT_TASK_EXPORT`
* 这3个宏有统一用法`INIT_HW_EXPORT(function, level)`，其中function就是入口函数，level是小优先级，填"0","1","2"...(引号要写的)，引号里数字越小，优先级越高。

* `INIT_HW_EXPORT`一般用于硬件初始化，GPIO之类的，可以没有
* `INIT_DRV_EXPORT`一般用于外设驱动初始化，初始化外部器件之类的，打开电源，提供时钟之类的，可以没有
* `INIT_TASK_EXPORT`一般用于初始化任务，用户代码基本上都是跑在任务里，原则上必须有

## 在SDK外存放项目文件的编译方法

假设项目路径是 `D:\github\ABCDEGF-webabc` , 目录结构如下

```tree
D:\github
    - ABCDEGF-webabc
        - code
            - xmake.lua
            - src
                app_main.c
                task_mqtt.c
            - inc
                app_main.h
                RTE_Device.h
        - doc
            - README.md
```

编译方式:

```cmd
set PROJECT_DIR=D:\github\ABCDEGF-webabc\code
build webabc
```

注意: code\xmake.lua里面的TARGET_NAME,对应命令行的项目名, `webabc`

恢复到默认项目查找逻辑, 之后就能恢复到 project/xxx 查找xmake.lua

```cmd
set PROJECT_DIR=
build luatos
```

## 在不联网环境下初次编译的补充说明

在有网的环境下, xmake会自行下载gcc工具链. 但如果无法联网, 或者网络受限的情况, 通常会有这种提示:

```cmd
error: fatal: not a git repository
```

或者git/http连接失败的提示. 故这里提供离线gcc工具链下载和编译的方法

1. 下载gcc for arm工具链[windows版](http://cdndownload.openluat.com/xmake/toolchains/gcc-arm/gcc-arm-none-eabi-10.3-2021.10-win32.zip)/[linux版](http://cdndownload.openluat.com/xmake/toolchains/gcc-arm/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2)
2. 解压, 不要选太深的目录, 不要包含中文字符和特殊符号, 建议解压到`D盘根目录`, 压缩包内自带一层目录`gcc-arm-none-eabi-10.3-2021.10`
3. 假设解压后的路径是 `D:\gcc-arm-none-eabi-10.3-2021.10`, 检查 `D:\gcc-arm-none-eabi-10.3-2021.10\bin\arm-none-eabi-g++.exe` 是否存在, 如果不存在, 那肯定是多一层目录. **务必检查!!!**
4. 用文本编辑器(例如vscode)打开 `本代码库` 的 `build.bat`, 修改内容如下

```cmd
原本的内容:
rem set GCC_PATH=E:\gcc_mcu
修改成set开头的语句,注意是去掉rem并修改值.
set GCC_PATH=D:\gcc-arm-none-eabi-10.3-2021.10
```

`重新打开`一个`命令行cmd`, 进入`本代码库`,执行 `build.bat` 即可正常编译.

5. 测试ok后, 可以考虑将上述环境变量GCC_PATH放入系统的环境变量设置中, 然后还原`build.bat`

## Linux下编译的额外的说明

安装Xmake可以使用官方提供的一键脚本

```shell
curl -fsSL https://xmake.io/shget.text | bash #使用curl进行安装
wget https://xmake.io/shget.text -O - | bash #使用wget进行安装
```

当前仅测试了 Ubuntu 16.04 和 Ubuntu 20.04 amd64版本

需要安装额外的32位支持, 否则执行fcelf时报文件不存. 后续可能会有64bit fcelf提供

```shell
dpkg --add-architecture i386 && apt update
apt-get install -y lib32z1 binutils:i386 libc6:i386 libgcc1:i386 libstdc++5:i386 libstdc++6:i386 p7zip-full
```

## macos 下编译的额外的说明

编译 wine 环境的 docker

```shell
./build-wine-docker.sh
```

## 授权协议

[MIT License](LICENSE)
