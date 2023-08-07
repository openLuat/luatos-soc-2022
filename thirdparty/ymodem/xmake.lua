
target("ymodem")
local LIB_DIR = "$(buildir)/cJymodemSON/"
set_kind("static")
set_targetdir(LIB_DIR)

--加入代码和头文件
add_includedirs("./",{public = true})
add_files("./luat_ymodem.c",{public = true})
add_includedirs(SDK_TOP .. "/thirdparty/ymodem",{public = true})
--路径可以随便写,可以加任意路径的代码,下面代码等效上方代码
-- add_includedirs(SDK_TOP .. "project/" .. TARGET_NAME .. "/inc",{public = true})
-- add_files(SDK_TOP .. "project/" .. TARGET_NAME .. "/src/*.c",{public = true})

--可以继续增加add_includedirs和add_files
--自动链接
LIB_USER = LIB_USER .. SDK_TOP .. LIB_DIR .. "libymodem.a "
target_end()
