@echo off
rem SOURCE: ticket #602 - le script de flash livre dans chaque artefact du firmware, pour Windows (gabarit : 4_Firmware/build.sh)
rem AUTHOR: engineer
rem DATE: 2026-10-11
rem STATUS: actif - copie par 4_Firmware/build.sh dans 4_Firmware/build/<cible>/artefact/, la cible ecrite a sa place, fins de ligne CRLF
rem
rem Usage : flash.cmd [PORT]        (PORT : COM3... ; sans lui, esptool cherche la carte)
rem
rem Ecrit le bootloader, la table de partitions et l'application que liste flash_args, rien d'autre : ni erase_flash,
rem ni l'image -full.bin, qui remplit de 0xFF la NVS et remettrait la marque a zero. La carte passe en telechargement
rem par la remise a zero d'esptool sur l'USB de la puce ; aucun bouton n'est suppose.
setlocal
cd /d "%~dp0"
set "PORT="
if not "%~1"=="" set "PORT=-p %~1"
set "E="
where esptool >nul 2>nul && set "E=esptool"
if not defined E (where esptool.py >nul 2>nul && set "E=esptool.py")
if not defined E (python -m esptool version >nul 2>nul && set "E=python -m esptool")
if not defined E (py -m esptool version >nul 2>nul && set "E=py -m esptool")
if not defined E (
    echo esptool introuvable : pip install esptool, ou l'executable autonome d'Espressif ^(https://github.com/espressif/esptool/releases^) dans ce dossier ou le PATH 1>&2
    exit /b 1
)
%E% --chip @CIBLE@ -b 460800 %PORT% write_flash @flash_args
exit /b %errorlevel%
