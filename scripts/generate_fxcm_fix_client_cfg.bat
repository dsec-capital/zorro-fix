@ECHO off &setlocal

SET INPUT=%1
SET OUTPUT=%2
SET SCRIPT_DIR=%~dp0

copy /Y %INPUT% %OUTPUT%

IF "%FIX_USER_NAME%" == "" (
  GOTO usage
)

IF "%FIX_PASSWORD%" == "" (
  GOTO usage
)

IF "%FIX_TARGET_SUBID%" == "" (
  GOTO usage
)

@CALL %SCRIPT_DIR%/replace_str.bat %OUTPUT% XXUSERNAMEXX %FIX_USER_NAME%
@CALL %SCRIPT_DIR%/replace_str.bat %OUTPUT% XXPWDXX %FIX_PASSWORD%
@CALL %SCRIPT_DIR%/replace_str.bat %OUTPUT% XXTARGETSUBIDXX %FIX_TARGET_SUBID%
@CALL %SCRIPT_DIR%/replace_str.bat %OUTPUT% XXACCOUNTIDXX %FIX_ACCOUNT_ID%

ECHO Generated FIX session configuration file %OUTPUT%
GOTO :eof

:usage
ECHO Did not generate session configuration file missing environment variables FIX_USER_NAME=%FIX_USER_NAME%, FIX_PASSWORD=%FIX_PASSWORD%, FIX_TARGET_SUBID=%FIX_TARGET_SUBID%
EXIT /B 1
