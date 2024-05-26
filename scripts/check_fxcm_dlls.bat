@echo off

SET SCRIPT_DIR=%~dp0

@CALL %SCRIPT_DIR%/exist_in_path "ForexConnect.dll"
@CALL %SCRIPT_DIR%/exist_in_path "pricehistorymgr.dll"
