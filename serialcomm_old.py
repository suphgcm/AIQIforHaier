import serial
import time

# 将16进制字符串转为byte数组
data = bytes.fromhex('FF 16 06 16 00 32 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 80 00 00 00 0B 7B B7 00 40 00 00 00 00 F7 A6 EA 00 00 C0 20 00 80 00 00 00 00 0B FB B7 00 40 00 00 00 00 F7 AD 84')

# 配置串口
ser = serial.Serial()
ser.port = 'COM7'
ser.baudrate = 38400
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
            # time.sleep(1)  # 给设备一些响应的时间
            print('数据发送成功')

            # 读取接收到的数据（如果有的话）
            # while ser.inWaiting():
            #     recv_data = ser.read(ser.inWaiting())
            #     print('接收到的数据是:', recv_data.hex())
        except Exception as e:
            print("出现错误：", str(e))
finally:
    ser.close()
    print('串口已关闭')