# 说明

* 片上flash读写演示

## 量产刷机时往指定flash地址刷入数据的说明

提醒:
1. 支持刷入多个区域
2. 需要使用 Multiownload_SinglePackage 下载, 普通的Multiownload不行
3. 工具下载地址: https://gitee.com/openLuat/LuatOS/attach_files

使用场景:
1. 刷入自定义的文件系统镜像
2. 刷入TTS数据到指定位置

## 操作步骤, 以单个区域为例

### 第一步, 确定刷入的地址/大小/名称

示例:

地址: 0x00A4D000 , 注意这是XIP地址, 也是程序中直接读取的位置
大小: 0x70000    , 载入文件的最大大小, 这里写的是448k
名称: SCRIPT     , 这个名字很重要, 可以自行修改, 但需要全大小

根据上述信息, 可以得到h文件需要修改的内容

```c
#define PKGFLXSCRIPT_IMG_MERGE_ADDR (0x00A4D000)
#define PKGFLXSCRIPT_FLASH_LOAD_SIZE (0x70000)
```

### 第二步, 修改mem_map.h

文件路径: `PLAT\device\target\board\ec618_0h00\common\inc\mem_map.h`

在结尾的`#endif` 之前, 添加上一步写好的h文件内容

### 第三步, 准备bin文件

bin文件就是准备写入到指定区域的数据, 没有格式要求, 全靠业务需要, 会原样刷入指定flash地址

这里假设名字叫 script.bin , 注意, 这个文件名后续还会用到

### 第四步, 合成binpkg文件

这里需要fcelf.exe, 在csdk和Multiownload_SinglePackage都能找到, 执行如下命令

```bat
fcelf.exe -M ^
-input out\example_flash\ap_bootloader.bin -addrname  BL_IMG_MERGE_ADDR -flashsize BOOTLOADER_FLASH_LOAD_SIZE ^
-input out\example_flash\ap.bin            -addrname  AP_IMG_MERGE_ADDR -flashsize AP_FLASH_LOAD_SIZE ^
-input out\example_flash\cp-demo-flash.bin -addrname  CP_IMG_MERGE_ADDR -flashsize CP_FLASH_LOAD_SIZE ^
-input out\example_flash\script.bin -addrname  PKGFLXSCRIPT_IMG_MERGE_ADDR -flashsize PKGFLXSCRIPT_FLASH_LOAD_SIZE ^
-def out\example_flash\mem_map.h ^
-outfile out\example_flash\merge.binpkg
```

命令解析:
1. `-M` 是合成binpkg的主命令
2. `-input X -addrname Y -flashsize Z` 是添加一个片段, 其中X是文件路径, Y和Z均为mem_map.h里面的宏
3. `-def X` 宏定义的头文件路径,按实际情况写
4. `-outfile X` 是输出路径, 后缀必须是binpkg

提醒:
1. 如果参数项写错, 有时候是不报错的!! 但写对就一定能生成正确的binpkg文件
2. 当前只有windows版本, 上游未提供linux版本!!!

### 第五步, 刷机

1. 必须用`Multiownload_SinglePackage`, 使用FlashTool和LuaTools均刷不了, 不要解压到深层目录, 不要带中文和特殊字符串的路径!!
2. 先把binpkg拷贝到进工具目录, 改名为 luatos.binpkg, 这是配置文件里写的名字, 建议调通了再改别的名字
3. 双击 `MulDownloader.exe` 启动, 选取配置文件 `config_ec618_usb_pkgsingle_rgmd.ini`
4. 点击 "Start Auto" 开始自动刷机
5. 操作模块, 使其进入BOOT模式, 工具就会自动刷机
6. 刷机完成后, 是不会自动重启的, 关掉工具, 按一下复位键, 就完成了
