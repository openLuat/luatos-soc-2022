set LUAT_EC618_LITE_MODE=1
build.bat luatos
set LUAT_EC618_LITE_MODE=0
del .\out\luatos\LuatOS-SoC_V1104_EC618_LITE.soc
copy .\out\luatos\LuatOS-SoC_V1104_EC618.soc .\out\luatos\LuatOS-SoC_V1104_EC618_LITE.soc
pause
