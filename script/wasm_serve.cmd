@echo off
cd %~dp0
cd ..
call yarn install
call node ./script/express.js
