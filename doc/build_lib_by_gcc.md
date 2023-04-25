#脱离本csdk编译无依赖的库文件

本文档阐述的是脱离csdk, 单独使用gcc链编译库文件的指南

1. 仅适合于完全不依赖本csdk的库
2. 对应的库除gcc提供的基础函数外, 对其他函数的依赖均通过无依赖的头文件进行声明

不符合以上条件的, 应仿照project/example, 编译成功后在 build/example 获取.a文件

## 所需的工具链

1. 下载[gcc for arm工具链](http://cdndownload.openluat.com/xmake/toolchains/gcc-arm/gcc-arm-none-eabi-10.3-2021.10-win32.zip)
2. 解压, 不要选太深的目录, 不要包含中文字符和特殊符号, 建议解压到`D盘根目录`, 压缩包内自带一层目录`gcc-arm-none-eabi-10.3-2021.10`
3. 假设解压后的路径是 `D:\gcc-arm-none-eabi-10.3-2021.10`, 检查 `D:\gcc-arm-none-eabi-10.3-2021.10\bin\arm-none-eabi-g++.exe` 是否存在, 如果不存在, 那肯定是多一层目录. **务必检查!!!**

## GCC编译所需要的参数

```
-mcpu=cortex-m3 
-mthumb 
-std=gnu99 
-nostartfiles 
-mapcs-frame 
-ffunction-sections 
-fdata-sections 
-fno-isolate-erroneous-paths-dereference 
-freorder-blocks-algorithm=stc 
-gdwarf-2
-mslow-flash-data
```

1. 优化级别推荐 `-O2` 或者 `-Os` , 但没有强制要求
2. gnu99是最低要求, 可以更高
