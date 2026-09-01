@echo off
setlocal

pushd "%~dp0.."
python -m tools.win_flasher %*
set EXIT_CODE=%ERRORLEVEL%
popd
exit /b %EXIT_CODE%
