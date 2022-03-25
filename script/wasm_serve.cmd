@echo off
cd %~dp0
cd ..
REM call yarn install
call node ./script/express.js
