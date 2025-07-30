@echo off
setlocal EnableDelayedExpansion

rem ---------------------------------------------
rem remove-junk.bat
rem Deletes all [.exe .dump .asm] files under a folder,
rem    with the option to keep specified filenames.
rem    keeps Masfix.exe
rem Usage:
rem    remove-junk.bat [targetDir] [keep1 keep2 ...]
rem Examples:
rem    remove-junk.bat examples fibonacci.exe number-guessing.dump
rem    remove-junk.bat KeepMe.exe            (uses current folder)
rem    remove-junk.bat                       (uses current folder, deletes all)
rem ---------------------------------------------

rem 1) Determine target directory and keep‑list
if "%~1"=="" (
    set "targetDir=%cd%"
    set "keepList="
) else (
    if exist "%~1\" (
        set "targetDir=%~1"
        shift
    ) else (
        set "targetDir=%cd%"
    )
    set "keepList=%*"
)

echo Target directory: "%targetDir%"
if defined keepList (
    echo Files to keep: %keepList%
) else (
    echo No files will be kept.
)
echo.

rem 2) Loop through every .exe and .dump (recursive)
for /r "%targetDir%" %%F in (*.exe *.dump *.asm) do (
    set "fname=%%~nxF"
    set "keepFlag=0"

	rem — implicitly protect Masfix.exe
	if /i "!fname!"=="Masfix.exe" set "keepFlag=1"

    rem 3) If user specified names to keep, check against each
    if defined keepList (
        for %%K in (!keepList!) do (
            if /i "%%~K"=="!fname!" (
                set "keepFlag=1"
            )
        )
    )

    rem 4) Delete or skip
    if "!keepFlag!"=="0" (
        echo Deleting "%%F"
        del "%%F"
    ) else (
        echo Skipping  "%%F"
    )
)

endlocal
