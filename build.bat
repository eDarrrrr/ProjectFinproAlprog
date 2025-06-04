@echo off
cd src
g++ -o ..\server.exe ServerNode.cpp -lws2_32
g++ -o ..\client.exe ClientNode.cpp -lws2_32
copy mahasiswa.csv ..\
copy log_harian.bin ..\
copy log_harian.json ..\
copy log.txt ..\
copy ClientNodeCamera.py ..\
echo Build selesai.
pause
