@echo off
setlocal EnableDelayedExpansion
cls
set app=SWRClock
set /p usedcom=Televersement via port COM
echo Utilisation de COM%usedcom%


echo Recherche de mkspiffs.exe
FOR /F "tokens=* USEBACKQ" %%G IN (`where /R C:\Users mkspiffs.exe`) DO (
echo %%G | findstr /r esp32 > nul
if errorlevel 1 (
    rem
) else (
	set ffs=%%G
)
)
echo Recherche de esptool.exe
FOR /F "tokens=* USEBACKQ" %%F IN (`where /R C:\Users esptool.exe`) DO (
SET tool=%%F
)
pause
cls
%ffs% -c ../data -p 256 -b 4096 -s 1048576 %app%.spiffs.bin
%tool% --chip esp32 --port com%usedcom% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x002F0000 %app%.spiffs.bin
del %app%.spiffs.bin
%tool% --chip esp32 --port COM%usedcom% --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0xe000 ../bin/WT32-SC01/boot_app0.bin 0x1000 ../bin/WT32-SC01/bootloader_qio_80m.bin 0x10000 ../bin/WT32-SC01/%app%WT32-SC01.ino.bin 0x8000 ../bin/WT32-SC01/%app%WT32-SC01.partitions.bin 

pause