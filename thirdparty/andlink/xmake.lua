target("libandlink")
    local LIB_DIR = "$(buildir)/libandlink/"
    set_kind("static")
    add_deps("luatos_lwip_socket")
    set_targetdir(LIB_DIR)

    includes(SDK_TOP .. "/thirdparty/libhttp",SDK_TOP .. "/thirdparty/libemqtt")
    add_deps("libhttp","libemqtt")

    add_includedirs(SDK_TOP .. "/thirdparty/cJSON",{public = true})
    add_files(SDK_TOP .. "/thirdparty/cJSON/**.c",{public = true})

    --加入代码和头文件
    add_includedirs("./",
                    SDK_TOP.."/luatos_lwip_socket/include",
    {public = true})

    add_files("./*.c",{public = true})

    --自动链接
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. "liblibandlink.a "
target_end()
