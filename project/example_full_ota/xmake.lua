local TARGET_NAME = "example_full_ota"
local LIB_DIR = "$(buildir)/".. TARGET_NAME .. "/"
local LIB_NAME = "lib" .. TARGET_NAME .. ".a "
includes(SDK_TOP.."/luatos_lwip_socket")
target(TARGET_NAME)
    set_kind("static")
    set_targetdir(LIB_DIR)
    add_deps("luatos_lwip_socket") --socket依赖

    --加入代码和头文件
    add_includedirs("./include",{public = true})
    add_files("./src/*.c",{public = true})
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. LIB_NAME .. " "
target_end()