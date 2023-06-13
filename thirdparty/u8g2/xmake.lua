
target("libu8g2")
    local LIB_DIR = "$(buildir)/libu8g2/"
    set_kind("static")

    set_targetdir(LIB_DIR)

    --加入代码和头文件
    add_includedirs("./",{public = true})
    add_files("./*.c",{public = true})

    --自动链接
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. "liblibu8g2.a "
target_end()
