# Hệ thống cảnh báo cháy AIoT PCCC qua MQTT

Dự án mô phỏng hệ thống giám sát và cảnh báo PCCC sử dụng ESP32 trên Wokwi.
Chương trình được viết bằng Arduino C++, đọc các cảm biến, điều khiển LED, còi,
LCD và trao đổi dữ liệu qua Wi-Fi/MQTT. MQTTX hoặc MQTT Explorer được dùng để
quan sát dữ liệu và gửi lệnh điều khiển từ máy tính.

## Chức năng

- DHT22 đo nhiệt độ và độ ẩm.
- Biến trở mô phỏng cảm biến khí gas từ 0 đến 100%.
- HC-SR04 đo khoảng cách tới mặt nước và tính phần trăm mực nước.
- LED xanh: an toàn; LED vàng: cảnh báo/lỗi cảm biến; LED đỏ: nguy hiểm.
- Còi kêu theo nhịp: bíp 250 ms, nghỉ 500 ms khi nhiệt độ `>= 60°C`,
  gas `>= 80%` hoặc mực nước `<= 25%`.
- Khi tất cả điều kiện nguy hiểm trở lại bình thường, còi tự tắt ở lần đọc
  cảm biến tiếp theo (tối đa khoảng 2 giây).
- Nút TEST trên Wokwi thử còi trong 5 giây.
- LCD và Serial Monitor hiển thị dữ liệu tại thiết bị.
- ESP32 publish dữ liệu MQTT và subscribe lệnh TEST/RESET.

## Ngưỡng trạng thái

| Trạng thái | Điều kiện |
|---|---|
| `SAFE` | Cảm biến hoạt động và không có điều kiện cảnh báo/nguy hiểm |
| `WARNING` | Nhiệt độ `>= 45°C` hoặc khí gas `>= 60%` |
| `DANGER` | Nhiệt độ `>= 60°C`, khí gas `>= 80%`, mực nước `<= 25%` hoặc đang chạy TEST |
| `FAULT` | DHT22 hoặc HC-SR04 trả về dữ liệu không hợp lệ |

Các điều kiện trong cùng một trạng thái được kết hợp bằng phép **HOẶC**. Vì vậy,
còi chỉ tắt khi không còn điều kiện `DANGER` nào và chế độ TEST đã kết thúc.

## Kết nối phần cứng

| Thiết bị | Chân ESP32 |
|---|---|
| DHT22 DATA | GPIO 15 |
| HC-SR04 TRIG | GPIO 5 |
| HC-SR04 ECHO | GPIO 18 |
| Biến trở mô phỏng khí gas | GPIO 34 |
| LED xanh | GPIO 25 |
| LED vàng | GPIO 26 |
| LED đỏ | GPIO 13 |
| Buzzer | GPIO 4 |
| Nút TEST | GPIO 27 |
| LCD I2C SDA/SCL | GPIO 21 / GPIO 22 |

## Mô hình IoT

```text
Cảm biến -> ESP32/Edge -> Wi-Fi -> MQTT Broker -> MQTTX
                ^                            |
                +---------- command --------+
```

| Thành phần | Vai trò |
|---|---|
| DHT22, biến trở, HC-SR04 | Tầng cảm nhận |
| ESP32 xử lý ngưỡng và điều khiển còi | Edge |
| Wokwi IoT Gateway | Gateway của simulator |
| Wokwi-GUEST | Mạng Wi-Fi |
| broker.hivemq.com | MQTT Broker |
| MQTTX/MQTT Explorer | Tầng ứng dụng, quan sát và gửi lệnh |

## Cấu trúc project

```text
AIoT_PCCC_MQTT/
├── include/config.h   # Wi-Fi, MQTT, chân GPIO và ngưỡng
├── src/main.cpp       # Toàn bộ chương trình C++
├── diagram.json       # Sơ đồ mạch Wokwi
├── platformio.ini     # Board và thư viện Arduino
├── wokwi.toml         # Firmware Wokwi sẽ chạy
└── README.md
```

`.pio/` là kết quả PlatformIO tự tạo sau khi build.

## Chạy Wokwi

1. Mở thư mục `AIoT_PCCC_MQTT` bằng VS Code.
2. Nhấn **PlatformIO: Build** và chờ `SUCCESS`.
3. Nhấn `F1`, chọn **Wokwi: Start Simulator**.
4. Mở Serial Monitor để theo dõi Wi-Fi, MQTT và số đo.

Sau mỗi lần sửa mã nguồn hoặc cấu hình, phải build lại và khởi động lại Wokwi
để simulator sử dụng firmware mới trong `.pio/build/esp32dev/`.

Nếu Serial không báo được IP, hãy bật Wokwi IoT Gateway/cho phép network của
Wokwi. ESP32 mô phỏng cần đường ra Internet để tới broker MQTT công cộng.

Kết nối thành công sẽ có các dòng tương tự:

```text
Connecting to Wokwi-GUEST...
Wi-Fi connected | IP=10.10.0.2 | RSSI=-60 dBm
MQTT connected
MQTT publish: {"temperature":24.00,...}
```

Nếu thấy `MQTT connected`, ESP32 đã đi qua Wi-Fi và kết nối được broker.

## Xem MQTT bằng MQTTX

Tải MQTTX Desktop cho Windows từ [trang tải chính thức](https://mqttx.app/downloads).

Tạo một connection mới:

```text
Name: PCCC Demo
Host: broker.hivemq.com
Port: 1883
Protocol: mqtt://
MQTT version: 3.1.1
Username/password: để trống
Client ID: đặt chuỗi riêng, ví dụ pccc-viewer-01
```

Sau khi Connect, subscribe các topic:

```text
aiot-pccc/demo-vn-2026/telemetry
aiot-pccc/demo-vn-2026/status
```

ESP32 publish telemetry khoảng 2 giây một lần:

```json
{
  "temperature": 25.0,
  "humidity": 40.0,
  "gas": 20.0,
  "water_level": 50.0,
  "status": "SAFE"
}
```

Để điều khiển ESP32, publish vào topic:

```text
aiot-pccc/demo-vn-2026/command
```

Payload là chuỗi thuần, không phải JSON:

```text
TEST
```

TEST làm còi kêu 5 giây. Gửi `RESET` để kết thúc thử sớm. Nếu cảm biến vẫn
nguy hiểm thì RESET không làm còi tắt, vì trạng thái vẫn là DANGER.

## Kịch bản demo

1. Để nhiệt độ, gas và nước bình thường: LED xanh, trạng thái SAFE.
2. Tăng nhiệt độ lên 50°C hoặc gas lên 65%: LED vàng, trạng thái WARNING.
3. Tăng nhiệt độ lên 65°C hoặc gas lên 85%: LED đỏ và còi kêu.
4. Hạ nhiệt độ dưới 60°C và gas dưới 80%: còi tự tắt nếu nước bình thường.
5. Đặt HC-SR04 ở 170 cm: nước khoảng 15%, LED đỏ và còi kêu.
6. Đặt lại 100 cm: nước khoảng 50%, còi tự tắt nếu các cảm biến khác bình thường.
7. Gửi TEST từ MQTTX: chứng minh MQTTX publish và ESP32 subscribe.
8. Xem telemetry cập nhật trong MQTTX: chứng minh ESP32 publish và MQTTX subscribe.

## Cách đọc code

Đọc `setup()` rồi `loop()` trong `src/main.cpp` trước:

1. `setup()` khởi tạo cảm biến, GPIO, LCD, Wi-Fi và MQTT.
2. `readSensors()` đọc ba loại cảm biến.
3. `getStatus()` trả về SAFE, WARNING, DANGER hoặc FAULT.
4. `updateOutputs()` điều khiển LED và còi.
5. `updateLCD()` hiển thị tại mạch.
6. `connectMqttIfNeeded()` chờ Wi‑Fi, kết nối broker và subscribe topic lệnh.
7. `publishTelemetry()` tạo JSON rồi publish lên broker.
8. `handleMqttCommand()` nhận TEST hoặc RESET từ MQTTX.

Các chân nối, topic và ngưỡng đều nằm trong `include/config.h` để dễ sửa.

## Lưu ý

Broker đang dùng là broker công cộng nên cần Internet và không dùng cho dữ liệu
nhạy cảm. Khi lắp mạch thật, chân ECHO 5V của HC-SR04 cần mạch chia áp hoặc level
shifter trước khi nối vào chân 3.3V của ESP32.
