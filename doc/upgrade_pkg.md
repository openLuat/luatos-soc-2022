# 升级包格式和生成说明(EC618适用)

本文档描述 `EC618` 系列模块各种固件的升级包格式和生成方法

## 术语

* 原始binpkg - 模块的原始固件，必然是`binpkg后缀`
* 原始差分包 - 使用移芯的FOTA工具, 通过2个binpkg文件生成差分包, 后缀为`.par`
* 合宙标准AT固件 - 除QAT外, 支持合宙标准AT指令的固件
* QAT固件 - 仅支持QAT指令的固件
* CSDK固件 - 本代码库(luatos-soc-2022)编译出的`非LuatOS`固件
* LuatOS固件 - 本代码库(luatos-soc-2022)编译出的`LuatOS`固件
* 差分升级 - 指底层CP/AP固件升级, 如果是`LuatOS`固件, 会包含脚本区数据
* 整包升级 - 用于特定CSDK固件, 包含完整CP/AP数据, 不依赖老版本, 不需要生成差分包

## 升级包格式列表

实际存在4种具体的差分包格式, 分别是:

1. 原始差分包 - 适用于 `QAT固件` / `CSDK固件(非整包升级)`
2. 合宙AT差分包 - 适用于`合宙标准AT固件`
3. CSDK整包升级包 - 适用于`CSDK固件(整包升级)`
4. LuatOS差分升级包 - 适用于`LuatOS固件`

## 生成方法 - 原始差分包

适合于`QAT固件` / `CSDK固件(非整包升级)`

1. 准备2个binpkg文件, 假设为`old.binpkg`和`new.binpkg`
2. 使用移芯FOTA工具, 生成`.par`文件, 假设为`diff.par`
3. 不添加任何前后数据, 原样下发给设备使用

## 生成方法 - 合宙AT差分包

适合于`合宙标准AT固件`, 例如 AT/LPAT/LSAT/AUAT

1. 使用 `原始差分包` 的生成方法, 先生成`diff.par`文件
2. 获取`diff.par`文件大小, 假设为`diff_size`
3. 生成文件头部数据, python形式的伪代码见下
4. 将上述数据附加到`diff.par`文件头部, 生成合宙AT格式的差分包

```python
head = struct.pack(">bIbI", 0x7E, 0x01, 0x7D, diff_size)
```

## 生成方法 - CSDK整包升级包

适合于`CSDK固件(整包升级)`, 这里仅描述大概的过程, 具体细节请参考dtools中的代码

1. 准备binpkg文件, 假设为`new.binpkg`, 不需要旧版本的binpkg文件
2. 使用`fcelf`解开binpkg, 得到`cp.bin`,`ap.bin`,`imagedata.json`
3. 使用`soc_tools.exe`, 将`cp.bin`压缩为`cp.zip`,实际为`lzma`格式, 分片大小256k
4. 使用`soc_tools.exe`, 将`ap.bin`压缩为`ap.zip`,实际为`lzma`格式, 分片大小256k
5. 将`cp.zip`和`ap.zip` 前后相连的方式, 合并为 `total.zip`
6. 准备一个0字节的空文件, 假设为`dummy.bin`
7. 执行`soc_tools.exe`, 将`total.zip`和`dummy.bin` 合成最终的升级包

具体实现代码请查阅 `tools\dtools`目录下的代码

## 生成方法 - LuatOS差分升级包

适合于`LuatOS固件`, 主要:

1. 不同的版本中, 底层可能是一致的, 即binpkg文件, 只是脚本区数据不同
2. 仅脚本区不一致, 也需要生成升级包
3. soc文件一般是7z格式或zip格式, 推荐用7za工具或者py7za库解开, 均能自动识别

生成步骤

1. 首先解开新老soc文件,得到binpkg
2. 对比新老binpkg文件, 若不相同, 生成`原始差分包`, 得到`diff.par`
3. 如果binpkg相同, 创建一个空的`diff.par`, 大小为0
4. 执行`soc_tools.exe`, 将`script.bin`压缩为 `script.zip`, 实际为`lzma`格式
5. 执行`soc_tools.exe`, 将`diff.par`和`script.zip` 合成最终的升级包

具体实现代码请查阅 `tools\dtools`目录下的代码

## 如何知晓生成何种升级包

对应EC618的升级包, 主要难点在于CSDK是差分升级还是整包升级

这里提供几个思路, 仅供参考

1. 方案A. 对应 `CSDK` 固件, 在编译固件时, 额外打包一个包含info.json的升级包文件, 通过`fota`表进行描述
2. 方案B, 创建iot项目时, 添加字段, 要求客户选择具体的类型

附info.json中fota表的片段, 其中的`core_type`标识了差分`diff`或整包`full`, 示例源于ec718库

```json
    "fota" : {
        "magic_num" : "eac37218",
        "block_len" : "40000", 
        "core_type" : "diff",
        "ap_type" : "diff",
        "cp_type" : "diff",
        "full_addr" : "002BD000",
        "fota_len" : "69000"
    }
```
