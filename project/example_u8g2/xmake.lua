local TARGET_NAME = "example_u8g2"
local LIB_DIR = "$(buildir)/".. TARGET_NAME .. "/"
local LIB_NAME = "lib" .. TARGET_NAME .. ".a "

target(TARGET_NAME)
    set_kind("static")
    set_targetdir(LIB_DIR)

    includes(SDK_TOP .. "/thirdparty/u8g2")
    add_deps("libu8g2")

    add_includedirs("./inc",{public = true})
    -- add_includedirs(SDK_TOP .. "/interface/private_include", 
    --                 {public = true})
    add_files("./src/*.c",{public = true})
    
    --自动链接
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. LIB_NAME .. " "
    --甚至可以加入自己的库
target_end()