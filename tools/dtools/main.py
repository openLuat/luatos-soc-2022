#!/usr/bin/python3
# -*- coding: UTF-8 -*-

import os, struct, sys, logging, subprocess, shutil

logging.basicConfig(level=logging.DEBUG)

resp_headers = {}

# 原版差分文件
def diff_org(old_path, new_path, dst_path):
    cmd = []
    if os.name != "nt":
        cmd.append("wine")
    cmd.append("FotaToolkit.exe")
    cmd.append("-d")
    if os.name != "nt":
        cmd.append("config/ec618.json")
    else:
        cmd.append("config\\ec618.json")
    cmd.append("BINPKG")
    cmd.append(dst_path)
    cmd.append(old_path)
    cmd.append(new_path)
    subprocess.check_call(" ".join(cmd), shell=True)

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

def merge_diff_script(diff_path, script_path, dst_path) :
    cmd = []
    if os.name != "nt":
        cmd.append("wine")
    cmd.append("soc_tools.exe")
    

# LuatOS文件的差分
def diff_soc(old_path, new_path, dst_path):
    tmpdir = "soctmp"
    if os.path.exists(tmpdir) :
        shutil.rmtree(tmpdir)
    os.makedirs(tmpdir)
    def tmpp(path) :
        return os.path.join(tmpdir, path)
    
    import py7zr, json
    old_param = None
    old_binpkg = None
    old_script = None
    new_param = None
    new_binpkg = None
    new_script = None
    with py7zr.SevenZipFile(old_path, 'r') as zip:
        for fname, bio in zip.readall().items():
            if str(fname).endswith("info.json") :
                fdata = bio.read()
                old_param = json.loads(fdata.decode('utf-8'))
            if str(fname).endswith(".binpkg") :
                old_binpkg = bio.read()
            if str(fname).endswith("script.bin") :
                old_script = bio.read()
    with py7zr.SevenZipFile(new_path, 'r') as zip:
        for fname, bio in zip.readall().items():
            if str(fname).endswith("info.json") :
                fdata = bio.read()
                new_param = json.loads(fdata.decode('utf-8'))
            if str(fname).endswith(".binpkg") :
                new_binpkg = bio.read()
            if str(fname).endswith("script.bin") :
                new_script = bio.read()
    if not old_param or not old_binpkg or not old_script:
        print("老版本不是SOC固件!!")
        return
    if not new_param or not new_binpkg or not new_script:
        print("新版本不是SOC固件!!")
        return
    script_only = new_binpkg == old_binpkg
    if script_only :
        with open("delta.par", "wb+") as f :
            pass
    else:
        with open(tmpp("old2.binpkg"), "wb+") as f :
            f.write(old_binpkg)
        with open(tmpp("new2.binpkg"), "wb+") as f :
            f.write(new_binpkg)
        diff_org(tmpp("old2.binpkg"), tmpp("new2.binpkg"), "delta.par")
    fstat = os.stat("delta.par")
    resp_headers["x-delta-size"] = str(fstat.st_size)
    
    with open(tmpp("script.bin"), "wb+") as f :
        f.write(new_script)
    # 先对script.bin进行打包
    # cmd = "{} zip_file {} {} \"{}\" \"{}\" {} 1".format(str(soc_exe_path), 
    # new_param['fota']['magic_num'], 
    # new_param["download"]["script_addr"], 
    # str(new_path.with_name(new_param["script"]["file"])), str(new_path.with_name("script_fota.zip")), str(new_param['fota']['block_len']))
    cmd = []
    if os.name != "nt":
        cmd.append("wine")
    cmd.append("soc_tools.exe")
    cmd.append("zip_file")
    cmd.append(str(new_param['fota']['magic_num']))
    cmd.append(str(new_param["download"]["script_addr"]))
    cmd.append(tmpp("script.bin"))
    cmd.append(tmpp("script_fota.zip"))
    cmd.append(str(new_param['fota']['block_len']))
    subprocess.check_call(" ".join(cmd), shell=True)

    ## 然后打包整体差分包
    # cmd = "{} make_ota_file {} 0 0 0 0 0 \"{}\" \"{}\" \"{}\"".format(str(soc_exe_path), 
    # new_param['fota']['magic_num'], 
    # str(new_path.with_name("script_fota.zip")), 
    # str(exe_path.with_name("delta.par")), 
    # str(Path(out_path, "output.sota")))
    cmd = []
    if os.name != "nt":
        cmd.append("wine")
    cmd.append("soc_tools.exe")
    cmd.append("make_ota_file")
    cmd.append(str(new_param['fota']['magic_num']))
    cmd.append("0")
    cmd.append("0")
    cmd.append("0")
    cmd.append("0")
    cmd.append("0")
    cmd.append(tmpp("script_fota.zip"))
    cmd.append("delta.par")
    cmd.append(tmpp("output.sota"))
    subprocess.check_call(" ".join(cmd), shell=True)

    shutil.copy(tmpp("output.sota"), dst_path)
    print("done soc diff")

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
    from bottle import request, post, static_file, response
    @post("/api/diff/<mode>")
    def http_api(mode):
        if os.path.exists("diff.bin"):
            os.remove("diff.bin")
        global resp_headers
        resp_headers.clear()
        oldBinbkg = request.files.get("old")
        newBinpkg = request.files.get("new")
        if os.path.exists("old.binpkg") :
            os.remove("old.binpkg")
        if os.path.exists("new.binpkg") :
            os.remove("new.binpkg")
        oldBinbkg.save("old.binpkg")
        newBinpkg.save("new.binpkg")
        do_mode(mode, "old.binpkg", "new.binpkg", "diff.bin", True)
        if len(resp_headers) > 0 :
            for k in resp_headers :
                response.add_header(k, resp_headers[k])
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
