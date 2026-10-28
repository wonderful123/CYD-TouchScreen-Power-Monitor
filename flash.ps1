$src = "\\wsl.localhost\Ubuntu\home\wonder\projects\esphome-cyd\.esphome\build\cyd-43\build\firmware.factory.bin"
$dst = "C:\esp"

New-Item -ItemType Directory -Force $dst | Out-Null
Copy-Item $src $dst -Force

python.exe -m esptool --chip esp32s3 --port COM7 --baud 460800 write-flash 0x0 "$dst\firmware.factory.bin"