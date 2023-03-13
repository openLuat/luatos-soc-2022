set LUAT_EC618_LITE_MODE=1
set LUAT_USE_TTS=1
build.bat luatos
set LUAT_EC618_LITE_MODE=
set LUAT_USE_TTS=
del .\out\luatos\LuatOS-SoC_V1104_EC618_TTS.soc
copy .\out\luatos\LuatOS-SoC_V1104_EC618.soc .\out\luatos\LuatOS-SoC_V1104_EC618_TTS.soc
pause
