import ctypes
import time
import serial
import argparse

# 加载dll库
lib = ctypes.CDLL('./libIoCtrlx64.dll')

# 配置dll函数参数类型和返回值类型
libIoCtrlInit = lib.libIoCtrlInit
libIoCtrlInit.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_char_p]
libIoCtrlInit.restype = ctypes.c_int

getPinLevel = lib.getPinLevel
getPinLevel.argtypes = [ctypes.POINTER(ctypes.c_void_p), ctypes.c_int, ctypes.POINTER(ctypes.c_ubyte)]
getPinLevel.restype = ctypes.c_int

# 初始化handle和bios_id
handle = ctypes.c_void_p()
bios_id = "C58.C57-H2.XX".encode('utf-8')

# 调用libIoCtrlInit函数
ret = libIoCtrlInit(ctypes.byref(handle), bios_id)
if ret != 0:
    print(f"Init lib failed: {ret}")
    exit(1)


def ReadMultipleTimes(index, times):
    state = ctypes.c_ubyte(2)
    ret = getPinLevel(ctypes.byref(handle), index, ctypes.byref(state))
    if ret != 0:
        print(f"Init lib failed: {ret}")
        return -1

    for i in range(times - 1):
        curState = ctypes.c_ubyte(2)
        ret = getPinLevel(ctypes.byref(handle), index, ctypes.byref(curState))
        if ret != 0:
            print(f"Init lib failed: {ret}")
            return -1
        if curState.value != state.value:
            # print("Level transition!")
            return -1

    return state.value


# 构建命令行参数解析器
parser = argparse.ArgumentParser()
parser.add_argument("port")
parser.add_argument("baudrate", type=int)
parser.add_argument("data")
args = parser.parse_args()

# 根据命令行参数读取值
data = bytes.fromhex(args.data)
port = args.port
baudrate = args.baudrate

# 配置串口
ser = serial.Serial()
ser.port = port
ser.baudrate = baudrate
ser.bytesize = serial.EIGHTBITS
ser.parity = serial.PARITY_NONE
ser.stopbits = serial.STOPBITS_ONE
ser.xonxoff = False


def sendMessage():
    try:
        ser.open()
        if ser.isOpen():
            print('成功打开串口')
            try:
                # 写入数据
                ser.write(data)
                print('数据发送成功')

                # 读取接收到的数据（如果有的话）
                # time.sleep(1)  # 给设备一些响应的时间
                # while ser.inWaiting():
                #     recv_data = ser.read(ser.inWaiting())
                #     print('接收到的数据是:', recv_data.hex())
            except Exception as e:
                print("出现错误：", str(e))
    finally:
        ser.close()
        print('串口已关闭')


def checkAndSend(index, requiredDur):
    timeMillisCount = int(time.time() * 1000)  # 获取当前时间的毫秒表示
    lastChange = timeMillisCount
    pinLevel = 1

    while True:
        curState = ReadMultipleTimes(index, 10)
        timeMillisCount = int(time.time() * 1000)  # 更新当前时间毫秒计数

        if curState == 0 and pinLevel == 1:
            # 高变低
            if timeMillisCount - lastChange < requiredDur:
                continue

            print('触发高变低')
            # 写入数据
            ser.write(data)
            print('数据发送成功')

            pinLevel = 0
            lastChange = timeMillisCount

        elif curState == 1 and pinLevel == 0:
            # 低变高
            if timeMillisCount - lastChange < requiredDur:
                continue

            print('触发低变高')

            pinLevel = 1
            lastChange = timeMillisCount


try:
    ser.open()
    if ser.isOpen():
        print('成功打开串口')

        checkAndSend(0, 300)

except Exception as e:
    print("出现错误：", str(e))

finally:
    ser.close()
    print('串口已关闭')

