local TARGET_NAME = "cloud_speaker"
local LIB_DIR = "$(buildir)/".. TARGET_NAME .. "/"
local LIB_NAME = "lib" .. TARGET_NAME .. ".a "
includes(SDK_TOP .. "/thirdparty/audio_decoder")
includes(SDK_TOP.."/luatos_lwip_socket")
target(TARGET_NAME)
    set_kind("static")
    set_targetdir(LIB_DIR)

    includes(SDK_TOP .. "/thirdparty/libemqtt")
    add_deps("libemqtt")

    -- add_includedirs("./", SDK_TOP .. "thirdparty/cjson")
    add_includedirs(SDK_TOP .. "/thirdparty/cJSON")
    add_files(SDK_TOP .. "/thirdparty/cJSON/**.c")

    add_deps("audio_decoder")
    -- includes(SDK_TOP .. "/thirdparty/cJSON")

    add_includedirs(SDK_TOP .. "/PLAT/core/tts/include/16k_lite_ver",{public = true})
    --加入自己代码和头文件
    add_includedirs("./inc",{public = true})
    add_files("./src/*.c",{public = true})

    --自动链接
    LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. LIB_NAME .. " "
    LIB_USER = LIB_USER .. SDK_TOP .. "/PLAT/core/lib/libaisound50_16K.a "
    --甚至可以加入自己的库
target_end()
