import serial
import argparse

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