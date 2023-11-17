# 启动脚本，用于启动c++生成的Injector.exe
# 用法：injector.exe game.exe path/to/dll.dll

import os

Injector = "C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\x64\\Debug\\GameInjector.exe"
Game = "RDR2.exe"
Dll = "C:\\Users\\Martini\\workspace\\VSrepos\\DTSS\\x64\\Debug\\GameHook.dll"


print("Launching...")
os.system(Injector + " " + Game + " " + Dll)