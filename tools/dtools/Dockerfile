FROM ubuntu:22.04



RUN sed -i 's/security.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/g' /etc/apt/sources.list
RUN sed -i 's/archive.ubuntu.com/mirrors.tuna.tsinghua.edu.cn/g' /etc/apt/sources.list

ENV TZ=Asia/Shanghai \
    DEBIAN_FRONTEND=noninteractive

COPY FotaToolkit.exe /opt/
COPY dep/ /opt/dep/
COPY *.dll /opt/
COPY config /opt/config/

COPY *.binpkg /opt/
# COPY attrib.exe /opt/

WORKDIR /opt/

RUN dpkg --add-architecture i386 &&\
    apt-get update &&\
    apt-get install -y wget &&\
    apt-get install -y wine32 wine64 &&\
    apt clean -y

# 调用一次FotaToolkit,这样才能初始化wine环境,不然启动耗时会很长
RUN wine FotaToolkit.exe -h || date