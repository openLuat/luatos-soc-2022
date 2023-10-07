#!/usr/bin/python3
# -*- coding: UTF-8 -*-

import os, struct, sys, logging, subprocess

logging.basicConfig(level=logging.DEBUG)

# 原版差分文件
def diff_org(old_path, new_path, dst_path):
    cmd = []
    if os.name != "nt":
        cmd.append("wine")
    cmd.append("FotaToolkit.exe")
    cmd.append("-d")
    cmd.append("config\ec618.json")
    cmd.append("BINPKG")
    cmd.append(dst_path)
    cmd.append(old_path)
    cmd.append(new_path)
    subprocess.check_call(cmd, shell=True)

# QAT固件比较简单, 原版差分文件
def diff_qat(old_path, new_path, dst_path):
    diff_org(old_path, new_path, dst_path)

# 合宙AT固件, 就需要添加一个头部
def diff_at(old_path, new_path, dst_path):
    # 先生成一个原始差分文件
    diff_org(old_path, new_path, dst_path)
    # 然后读取数据
    with open(dst_path, "rb") as f:
        data = f.read()
    # 添加头部,生成最终文件
    with open(dst_path, "wb") as f:
        #写入头部
        head = struct.pack(">bIbI", 0x7E, 0x01, 0x7D, len(data))
        f.write(head)
        #写入原始数据
        f.write(data)

# CSDK也是原始差分
def diff_csdk(old_path, new_path, dst_path):
    diff_org(old_path, new_path, dst_path)

# TODO LuatOS文件的差分
def diff_soc(old_path, new_path, dst_path):
    pass

def do_mode(mode, old_path, new_path, dst_path, is_web) :

    # 根据不同的模式执行
    if mode == "org":
        diff_org(old_path, new_path, dst_path)
    elif mode == "qat" :
        diff_qat(old_path, new_path, dst_path)
    elif mode == "at" :
        diff_at(old_path, new_path, dst_path)
    elif mode == "csdk" :
        diff_csdk(old_path, new_path, dst_path)
    elif mode == "soc" :
        diff_soc(old_path, new_path, dst_path)
    else:
        print("未知模式, 未支持" + mode)
        if not is_web :
            sys.exit(1)

def start_web():
    import bottle
    from bottle import request, post, static_file
    @post("/api/diff/<mode>")
    def http_api(mode):
        if os.path.exists("diff.bin"):
            os.remove("diff.bin")
        oldBinbkg = request.files.get("old")
        newBinpkg = request.files.get("new")
        oldBinbkg.save("old.binpkg")
        newBinpkg.save("new.binpkg")
        do_mode(mode, "old.binpkg", "new.binpkg", "diff.bin", True)
        return static_file("diff.bin", root=".", download="diff.bin")
    bottle.run(host="0.0.0.0", port=9000)

def main():
    if len(sys.argv) < 2 :
        return
    mode = sys.argv[1]
    if mode == "web" :
        start_web()
        return
    if len(sys.argv) < 5 :
        print("需要 模式 老版本路径 新版本路径 目标输出文件路径")
        print("示例:  python main.py qat old.binpkg new.binpkg diff.bin")
        print("可选模式有: at qat csdk org")
        sys.exit(1)
    do_mode(mode, sys.argv[2], sys.argv[3], sys.argv[4], False)

if __name__ == "__main__":
    main()
