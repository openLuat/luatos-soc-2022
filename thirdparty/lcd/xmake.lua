
target("liblcd")
    local LIB_DIR = "$(buildir)/liblcd/"
    set_kind("static")

    set_targetdir(LIB_DIR)

    --加入代码和头文件
    add_includedirs("./",
                    "../qrcode",
    {public = true})

    add_files("./*.c",
                "../qrcode/*.c",
    {public = true})

    --自动链接
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. "libliblcd.a "
target_end()
