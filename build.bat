@echo off
cd src
g++ -o ..\server.exe ServerNode.cpp -lws2_32
g++ -o ..\client.exe ClientNode.cpp -lws2_32
echo Build selesai.
pause
