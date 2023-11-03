# 差分包生成工具

本文件夹是生成差分包所需要的工具, 分2部分:
1. windows原生工具
2. 可运行原生工具的docker镜像

## 原生工具的用法

先把需要差分的新老binpkg, 分别命名为 `old.binpkg` 和 `new.binpkg`, 存放在当前目录下.

### 直接跑命令行, 适合 QAT固件和CSDK固件

```shell
FotaToolkit.exe -d config\ec618.json BINPKG old.binpkg new.binpkg diff.bin
```

生成完成后, 得到diff.bin, 注意, 文件大小超过(FOTA分区大小-32k系统占用), 就无法成功更新

FOTA分区的默认大小是512K, f所以默认情况下只能480K的bin文件

### 通过脚本跑

```
python3 main.py csdk old.binpkg new.binpkg diff.bin
```

其中csdk是模式, 可选值有`csdk` `at` `qat` `org` `soc`


## Docker镜像说明

鉴于服务器大多是linux系统,而fota工具又没有linux系统, 这里提供docker镜像

构建镜像
```
docker build -t ecfota .
```

运行镜像, 暴露9000端口, 会创建一个http api
```
docker run -it --rm -p 9000:9000 ecfota
```

### http api调用规则

```
URL         /api/diff/<mode>
METHOD      POST
使用文件上传的方式 老文件的参数名 old, 新文件的参数名 new
响应结果是diff.bin
<mode> 是模式, 与命令行的模式一致
```

注意: docker镜像的web服务也是 `main.py` 提供的,非必须, 其他编程语言也可以直接在镜像内调用差分工具

## 其他信息

未尽事宜,请咨询FAE或销售
