import uvicorn
import json
from fastapi import FastAPI
import ctypes
import time
import argparse

# 读取持续时间
parser = argparse.ArgumentParser()
parser.add_argument("port", type=int)
parser.add_argument("duration", type=int)
args = parser.parse_args()

port = args.port
duration = args.duration

# 加载dll库
lib = ctypes.CDLL('./libIoCtrlx64.dll')

# 配置dll函数参数类型和返回值类型
libIoCtrlInit = lib.libIoCtrlInit
libIoCtrlInit.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_char_p]
libIoCtrlInit.restype = ctypes.c_int

setPinLevel = lib.setPinLevel
setPinLevel.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int, ctypes.c_int]
setPinLevel.restype = ctypes.c_int

# 初始化handle和bios_id
handle = ctypes.c_void_p()
bios_id = "C58.C57-H2.XX".encode('utf-8')

# 调用libIoCtrlInit函数
ret = libIoCtrlInit(ctypes.byref(handle), bios_id)
if ret != 0:
    print(f"Init lib failed: {ret}")
    exit(1)

# 服务器
app = FastAPI()


@app.post("/alarm")
def alarm(data: dict):
    pipelineCode = data['pipelineCode']
    productSn = data['productSn']
    print('pipelineCode: ', pipelineCode)
    print('productSn: ', productSn)

    setPinLevel(ctypes.byref(handle), 7, 1)
    time.sleep(duration)
    setPinLevel(ctypes.byref(handle), 7, 0)

    return {"msg": json.dumps(data)}


if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=port)
