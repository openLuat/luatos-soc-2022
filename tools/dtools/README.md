# 差分包生成工具

本文件夹是生成差分包所需要的工具, 分2部分:

1. windows原生工具
2. 可运行原生工具的docker镜像

支持的差分功能:

1. CSDK生产的binpkg差分,功能名称csdk
2. 合宙系列AT固件的差分,功能名称at
3. QAT系列固件的差分,功能名称qat
4. LuatOS固件的soc文件之间的差分,功能名称soc
5. 原始SDK的binpkg差分,功能名称org
6. CSDK全量/整包升级包,功能名称full

演示地址: https://ecfota-ec618.k8s.air32.cn/

## 原生工具的用法

### 通过脚本跑

如果是LuatOS的soc文件差分,需要先安装依赖项

```
pip3 install -i https://pypi.tuna.tsinghua.edu.cn/simple py7zr
```

差分命令格式及示例

```
python3 main.py 模式 老版本固件路径 新版本固件路径 输出差分包的路径

python3 main.py csdk old.binpkg new.binpkg diff.bin
```

其中
* 模式, 可选值有`csdk` `at` `qat` `org` `soc` `full`
* 非LuatOS固件传binpkg路径, LuatOS固件传soc文件路径

|差分模式|文件后缀|说明|
|-------|-------|----|
|org    |binpkg |移芯原始sdk的差分包|
|at     |binpkg |合宙AT固件差分包,包括LSAT,AUAT,LPAT固件|
|qat    |binpkg |合宙QAT固件差分包|
|csdk   |binpkg |本CSDK所使用的差分包|
|soc    |soc    |LuatOS固件的差分包|
|full   |binpkg |CSDK全量/整包升级包|

## Docker镜像说明

鉴于服务器大多是linux系统,而fota工具又没有linux系统, 这里提供docker镜像

拉取最新镜像(可选)
```
docker pull wendal/ecfota-ec618
```

或者构建本地镜像(可选)
```
docker build -t wendal/ecfota-ec618 .
```

运行镜像, 暴露9000端口, 会创建一个http api
```
docker run -it --rm -p 9000:9000 wendal/ecfota-ec618
```

本目录也提供了docker-compose和k8s的部署文件

### http api调用规则

```
URL         /api/diff/<mode>
METHOD      POST
使用文件上传的方式 

可用参数:
old 老版本的文件
new 新版本的文件
oldurl 老版本的URL,将通过该URL获取老版本的文件
newurl 新版本的URL,将通过该URL获取新版本的文件
mode   替代URL中的mode参数, 与命令行的模式一样
<mode> URL参, 是模式, 与命令行的模式一样
```

注意, old和oldurl需提供其中一个, new和newurl也需提供其中一个 

注意: docker镜像的web服务也是 `main.py` 提供的,非必须, 其他编程语言也可以直接在镜像内调用差分工具

## 其他信息

* [安装wine9的教程](install_wine.md)

未尽事宜,请咨询FAE或销售

## 更新日志

# 1.0.7

1. 支持csdk的全量升级包

# 1.0.5

1. prometheus数据集成到web服务中,不需要单独一个端口

# 1.0.4

1. 支持多线程请求
2. 支持心跳检测
3. 支持prometheus

### 1.0.3

1. 增加腾讯云COS集成, 支持差分包重用
2. 新增oldurl,newurl参数

### 1.0.1

1. 增加mode参数, 替代URL中的mode参数
