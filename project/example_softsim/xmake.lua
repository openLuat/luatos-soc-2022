local TARGET_NAME = "example_softsim"
local LIB_DIR = "$(buildir)/".. TARGET_NAME .. "/"
local LIB_NAME = "lib" .. TARGET_NAME .. ".a "
local LUATOS_ROOT = SDK_TOP .. "/../LuatOS/"
includes(SDK_TOP.."/luatos_lwip_socket")
target(TARGET_NAME)
    set_kind("static")
    set_targetdir(LIB_DIR)
    add_deps("luatos_lwip_socket")
    --加入代码和头文件
    add_includedirs("./include",{public = true})
    add_files("./src/*.c",{public = true})

    --可以继续增加add_includedirs和add_files
    --自动链接
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. LIB_NAME .. " "
    LIB_USER = LIB_USER .. SDK_TOP .. "/lib/libtgt_app_service.a "

target_end()