@echo off
set PROJECT_NAME=example
set LSPD_MODE=disable
rem you can set your gcc path
rem set GCC_PATH=E:\gcc_mcu
set ROOT_PATH = %cd:\=/%
if not "%1"=="" (
	set PROJECT_NAME=%1
) ELSE (
	@echo PROJECT not set
)


if not "%2"=="" (
	set LSPD_MODE=%2
) ELSE (
	if "%1" == "luatos" (
		set LSPD_MODE=enable
	) ELSE (
		@echo LSPD_MODE not set
	)
)


rem check csdk rndis
set RNDIS_MARK="csdk_rndis"
if exist %RNDIS_MARK% (
	@echo This is CSDK with RNDIS
	set EC618_RNDIS=enable
	set LSPD_MODE=disable
) else (
	set EC618_RNDIS=disable
)

@echo "=============================="
@echo "AirM2M https://openluat.com"
@echo "=============================="
@echo PROJECT   : %PROJECT_NAME%  
@echo LSPD_MODE : %LSPD_MODE%  
@echo RNDIS     : %EC618_RNDIS%
@echo "=============================="

if not exist xmake.lua (
	echo xmake.lua not exist
	goto end
)
if "%3"=="-v" (
	call xmake -v
) ELSE (
	call xmake -w
)

:end
echo end
pause

