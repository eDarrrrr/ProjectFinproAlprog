# ================================
# NOTE:
# Agar kode ini berjalan, pastikan sudah install:
# pip install opencv-python pyzbar
# Lalu jalankan menggunakan comman python ClientNodeCamera.py
# ================================

import cv2
from pyzbar.pyzbar import decode
import socket

SERVER_IP = '127.0.0.1'
PORT = 8888
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
cap = cv2.VideoCapture(0)

sock.connect((SERVER_IP, PORT))

print("Arahkan barcode ke kamera (tekan 'q' untuk keluar)")
while True:
    ret, frame = cap.read()
    if not ret:
        break

    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    # Decode semua barcode dalam frame
    eq = cv2.equalizeHist(gray)
    barcodes = decode(eq)
    for barcode in barcodes:
        npm = barcode.data.decode('utf-8')
        print(f'Barcode terdeteksi: {npm}')
        sock.send(npm.encode('utf-8'))
        cv2.rectangle(frame, (barcode.rect.left, barcode.rect.top),
                      (barcode.rect.left + barcode.rect.width, barcode.rect.top + barcode.rect.height),
                      (0, 255, 0), 2)
        cv2.waitKey(2000)  # delay supaya tidak kirim berkali-kali
        break

    cv2.imshow('Barcode Scanner', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
