@ECHO off

SETLOCAL enableextensions disabledelayedexpansion

SET "textFile=%1"
SET "search=%2"
SET "replace=%3"

ECHO Replacing %search% with %replace% 

FOR /f "delims=" %%i in ('type "%textFile%" ^& break ^> "%textFile%" ') do (
    SET "line=%%i"
    SETLOCAL enabledelayedexpansion
    >>"%textFile%" echo(!line:%search%=%replace%!
    ENDLOCAL
)
