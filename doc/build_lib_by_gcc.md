# 脱离本csdk编译无依赖的库文件

本文档阐述的是脱离csdk, 单独使用gcc链编译库文件的指南

1. 仅适合于完全不依赖本csdk的库
2. 对应的库除gcc提供的基础函数外, 对其他函数的依赖均通过无依赖的头文件进行声明

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

## 生成的.a文件如何使用

1. 假定名称为`libabc.a`, 将.a文件放入 本SDK根目录下的`lib`目录 , 与`libgt.a`一样
2. 在项目目录的xmake.lua中,添加如下语句

```lua
LIB_USER = LIB_USER .. SDK_TOP .. "/lib/libabc.a "
```

## 额外说明, 依赖本csdk生成.a文件的方法

1. 应仿照project/example, 将源文件和头文件都src/include目录
2. 按常规方式编译, 例如 `build example`
3. 编译成功后在 build/example 获取`.a`文件
