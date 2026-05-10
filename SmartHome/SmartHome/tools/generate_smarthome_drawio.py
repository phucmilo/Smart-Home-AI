#!/usr/bin/env python3
from __future__ import annotations

from datetime import datetime
from pathlib import Path
import uuid
import xml.etree.ElementTree as ET
from xml.dom import minidom
import re


OUT = Path("Docs/SmartHome_Diagrams.drawio")


STYLES = {
    "title": "text;html=1;strokeColor=none;fillColor=none;whiteSpace=wrap;align=left;verticalAlign=top;fontSize=26;fontStyle=1;fontColor=#111827;",
    "subtitle": "text;html=1;strokeColor=none;fillColor=none;whiteSpace=wrap;align=left;verticalAlign=top;fontSize=13;fontColor=#475569;",
    "note": "rounded=1;whiteSpace=wrap;html=1;fillColor=#f8fafc;strokeColor=#94a3b8;dashed=1;fontSize=12;align=left;verticalAlign=top;spacing=10;",
    "start": "ellipse;whiteSpace=wrap;html=1;fillColor=#dcfce7;strokeColor=#16a34a;fontSize=12;fontStyle=1;",
    "end": "ellipse;whiteSpace=wrap;html=1;fillColor=#fee2e2;strokeColor=#dc2626;fontSize=12;fontStyle=1;",
    "process": "rounded=1;whiteSpace=wrap;html=1;fillColor=#dbeafe;strokeColor=#3b82f6;fontSize=12;spacing=8;",
    "process2": "rounded=1;whiteSpace=wrap;html=1;fillColor=#e0f2fe;strokeColor=#0284c7;fontSize=12;spacing=8;",
    "io": "shape=parallelogram;perimeter=parallelogramPerimeter;whiteSpace=wrap;html=1;fillColor=#dcfce7;strokeColor=#16a34a;fontSize=12;spacing=8;",
    "decision": "rhombus;whiteSpace=wrap;html=1;fillColor=#fef3c7;strokeColor=#d97706;fontSize=12;spacing=8;",
    "data": "rounded=1;whiteSpace=wrap;html=1;fillColor=#ede9fe;strokeColor=#7c3aed;fontSize=12;spacing=8;",
    "block": "rounded=1;whiteSpace=wrap;html=1;fillColor=#e2e8f0;strokeColor=#64748b;fontSize=12;spacing=8;",
    "block_green": "rounded=1;whiteSpace=wrap;html=1;fillColor=#dcfce7;strokeColor=#16a34a;fontSize=12;spacing=8;",
    "block_yellow": "rounded=1;whiteSpace=wrap;html=1;fillColor=#fef3c7;strokeColor=#d97706;fontSize=12;spacing=8;",
    "block_red": "rounded=1;whiteSpace=wrap;html=1;fillColor=#fee2e2;strokeColor=#dc2626;fontSize=12;spacing=8;",
    "block_purple": "rounded=1;whiteSpace=wrap;html=1;fillColor=#ede9fe;strokeColor=#7c3aed;fontSize=12;spacing=8;",
    "mcu": "rounded=1;whiteSpace=wrap;html=1;fillColor=#111827;strokeColor=#374151;fontColor=#ffffff;fontSize=12;fontStyle=1;spacing=10;",
    "rail3": "rounded=1;whiteSpace=wrap;html=1;fillColor=#bbf7d0;strokeColor=#16a34a;fontSize=12;fontStyle=1;",
    "rail5": "rounded=1;whiteSpace=wrap;html=1;fillColor=#fecaca;strokeColor=#dc2626;fontSize=12;fontStyle=1;",
    "rail12": "rounded=1;whiteSpace=wrap;html=1;fillColor=#fed7aa;strokeColor=#ea580c;fontSize=12;fontStyle=1;",
    "railg": "rounded=1;whiteSpace=wrap;html=1;fillColor=#e5e7eb;strokeColor=#111827;fontSize=12;fontStyle=1;",
    "group": "rounded=1;whiteSpace=wrap;html=1;fillColor=none;strokeColor=#cbd5e1;dashed=1;fontSize=12;",
    "edge": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#334155;strokeWidth=2;fontSize=11;",
    "edge_signal": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#2563eb;strokeWidth=2;fontSize=10;",
    "edge_power3": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#16a34a;strokeWidth=2;fontSize=10;",
    "edge_power5": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#dc2626;strokeWidth=2;fontSize=10;",
    "edge_power12": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#ea580c;strokeWidth=2;fontSize=10;",
    "edge_gnd": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#111827;strokeWidth=2;fontSize=10;",
    "edge_dash": "edgeStyle=orthogonalEdgeStyle;rounded=0;orthogonalLoop=1;jettySize=auto;html=1;endArrow=block;strokeColor=#64748b;strokeWidth=2;dashed=1;fontSize=10;",
}


def br(text: str) -> str:
    return text.replace("\n", "<br>")


class Diagram:
    def __init__(self, name: str, page_width: int = 1500, page_height: int = 1050):
        self.name = name
        self.prefix = re.sub(r"[^A-Za-z0-9_]+", "_", name).strip("_")[:24] or "page"
        self.page_width = page_width
        self.page_height = page_height
        self.counter = 0
        self.model = ET.Element(
            "mxGraphModel",
            {
                "dx": "1422",
                "dy": "794",
                "grid": "1",
                "gridSize": "10",
                "guides": "1",
                "tooltips": "1",
                "connect": "1",
                "arrows": "1",
                "fold": "1",
                "page": "1",
                "pageScale": "1",
                "pageWidth": str(page_width),
                "pageHeight": str(page_height),
                "math": "0",
                "shadow": "0",
            },
        )
        self.root = ET.SubElement(self.model, "root")
        ET.SubElement(self.root, "mxCell", {"id": "0"})
        ET.SubElement(self.root, "mxCell", {"id": "1", "parent": "0"})

    def new_id(self) -> str:
        self.counter += 1
        return f"{self.prefix}_{self.counter}"

    def vertex(self, label: str, x: int, y: int, w: int, h: int, style: str = "process") -> str:
        cid = self.new_id()
        cell = ET.SubElement(
            self.root,
            "mxCell",
            {
                "id": cid,
                "value": br(label),
                "style": STYLES[style],
                "vertex": "1",
                "parent": "1",
            },
        )
        ET.SubElement(
            cell,
            "mxGeometry",
            {"x": str(x), "y": str(y), "width": str(w), "height": str(h), "as": "geometry"},
        )
        return cid

    def text(self, label: str, x: int, y: int, w: int, h: int, style: str = "subtitle") -> str:
        return self.vertex(label, x, y, w, h, style)

    def edge(self, source: str, target: str, label: str = "", style: str = "edge") -> str:
        cid = self.new_id()
        cell = ET.SubElement(
            self.root,
            "mxCell",
            {
                "id": cid,
                "value": br(label),
                "style": STYLES[style],
                "edge": "1",
                "parent": "1",
                "source": source,
                "target": target,
            },
        )
        ET.SubElement(cell, "mxGeometry", {"relative": "1", "as": "geometry"})
        return cid


def header(d: Diagram, title: str, subtitle: str) -> None:
    d.text(title, 40, 28, d.page_width - 80, 38, "title")
    d.text(subtitle, 42, 68, d.page_width - 84, 38, "subtitle")


def source_note(d: Diagram, text: str) -> None:
    d.vertex(text, 40, d.page_height - 98, d.page_width - 80, 62, "note")


def vertical_flow(d: Diagram, x: int, y: int, items: list[tuple[str, str, int]], gap: int = 24, w: int = 300):
    ids = []
    cy = y
    for label, style, h in items:
        ids.append(d.vertex(label, x, cy, w, h, style))
        cy += h + gap
    for a, b in zip(ids, ids[1:]):
        d.edge(a, b)
    return ids


def page_overview() -> Diagram:
    d = Diagram("BLOCK - Tổng quan dự án", 1550, 1050)
    header(d, "Sơ đồ khối tổng quan dự án SmartHome", "Các ESP32 giao tiếp qua MQTT Broker; WebApp và Home Assistant là lớp điều khiển/giám sát.")
    user = d.vertex("Người dùng\nTrình duyệt / điện thoại", 50, 170, 180, 90, "block")
    web = d.vertex("WebApp Node.js\nExpress + WebSocket\nUI điều khiển cửa, đèn, gas,\nRFID, vân tay, camera", 300, 145, 230, 140, "block_purple")
    ha = d.vertex("Home Assistant\nMQTT lock entity\nLovelace dashboard", 305, 365, 220, 105, "block_purple")
    broker = d.vertex("MQTT Broker Mosquitto\nDocker trên máy Mac/LAN\nKhông đưa mật khẩu vào sơ đồ", 620, 230, 250, 135, "data")
    main = d.vertex("ESP32 Main\nKhóa cửa + cảm biến\nMQTT client: ESP32_DOOR", 980, 100, 235, 120, "mcu")
    voice = d.vertex("ESP32 Voice\nI2S mic + AI/VAD\nMQTT client: ESP32_VOICE", 980, 310, 235, 115, "mcu")
    cam = d.vertex("ESP32-CAM AI Thinker\nOV2640 + Face Recognition\nMQTT client: ESP32_CAM", 980, 530, 235, 130, "mcu")
    door_hw = d.vertex("Cửa / Servo\nSG90/MG90S\nGPIO2 PWM", 1280, 80, 180, 75, "block_green")
    access = d.vertex("RFID + Keypad +\nFingerprint + Touch", 1280, 170, 180, 90, "block_green")
    gas = d.vertex("MQ Gas + LCD I2C + Buzzer\nADC34, SDA0, SCL22, GPIO12", 1280, 275, 210, 100, "block_yellow")
    light = d.vertex("Đèn / LED\nGPIO5 trên ESP32 Voice", 1280, 405, 180, 75, "block_green")
    camera_ui = d.vertex("MJPEG Stream + Web UI Camera\nHTTP từ ESP32-CAM", 1280, 555, 210, 85, "block_yellow")

    d.edge(user, web, "")
    d.edge(user, ha, "")
    d.edge(web, broker, "MQTT")
    d.edge(ha, broker, "lock topic")
    d.edge(broker, main, "door/keypad/RFID/fingerprint/gas")
    d.edge(broker, voice, "light")
    d.edge(broker, cam, "cam/face")
    d.edge(web, cam, "HTTP stream/control", "edge_dash")
    d.edge(cam, broker, "MATCH -> face_result\n+ UNLOCK")
    d.edge(main, door_hw, "lock/unlock")
    d.edge(access, main, "local auth inputs", "edge_signal")
    d.edge(main, gas, "read/display/alert")
    d.edge(voice, light, "ON/OFF")
    d.edge(cam, camera_ui, "video + face result")
    d.vertex("Ghi chú: WebApp cũng có nhận diện giọng nói bằng Web Speech API trong trình duyệt để gửi lệnh cửa/đèn qua WebSocket -> MQTT.", 55, 735, 635, 92, "note")
    d.vertex("Nguồn cấp tổng quát\n5V: ESP32/servo/camera/amp/LCD/MQ\n3.3V: module logic nhạy như RC522, INMP441\n12V: module ngoài chỉ hiển thị nhiệt độ, không nối GPIO", 780, 735, 635, 118, "note")
    source_note(d, "Nguồn thông tin: Esp32_Main/main/*.ino, Esp32_Voice/Voice/*.ino, Esp32_Cam/Web_server/*.ino, WebApp/server.js, HomeAssistant/ha-config.")
    return d


def page_mqtt_topics() -> Diagram:
    d = Diagram("BLOCK - MQTT topics", 1600, 1180)
    header(d, "Sơ đồ khối MQTT topics", "Luồng publish/subscribe chính giữa WebApp, Home Assistant, ESP32 Main, ESP32 Voice và ESP32-CAM.")
    pubs_x, topic_x, subs_x = 55, 485, 1030
    d.vertex("Publisher", pubs_x, 115, 300, 45, "block")
    d.vertex("Topic / Payload", topic_x, 115, 430, 45, "data")
    d.vertex("Subscriber / Hành động", subs_x, 115, 430, 45, "block")
    rows = [
        ("WebApp / Home Assistant / CAM", "home/door/command\nUNLOCK | LOCK", "ESP32 Main\nServo mở/khóa cửa"),
        ("ESP32 Main", "home/door/state\nlocked | unlocked (retained)", "WebApp + Home Assistant\nCập nhật trạng thái"),
        ("WebApp", "home/light/command\nON | OFF", "ESP32 Voice\nĐiều khiển LED GPIO5"),
        ("ESP32 Voice", "home/light/state\nON | OFF (retained)", "WebApp\nCập nhật trạng thái đèn"),
        ("WebApp", "home/finger/enroll, home/finger/delete\nID 1..127", "ESP32 Main\nEnroll/delete DY50"),
        ("ESP32 Main", "home/finger/status\nplace_finger, ok:id, fail:*", "WebApp\nHiển thị tiến trình vân tay"),
        ("WebApp", "home/rfid/cmd\nadd_next | remove:UID", "ESP32 Main\nThêm/xóa thẻ RC522"),
        ("ESP32 Main", "home/rfid/scan\nok/denied/added/already:UID", "WebApp\nLịch sử RFID"),
        ("WebApp", "home/keypad/cmd\nsetpass:xxxx", "ESP32 Main\nĐổi mật khẩu keypad"),
        ("ESP32 Main", "home/keypad/status\nok, wrong, clear, changed", "WebApp\nCập nhật keypad"),
        ("ESP32 Main", "home/gas\nADC raw retained", "WebApp\nHiển thị mức khí gas"),
        ("ESP32-CAM", "home/door/face_result\nJSON MATCH/NO_MATCH/ENROLLED", "WebApp\nNếu MATCH -> publish UNLOCK"),
        ("WebApp", "home/cam/cmd\nJSON capture/enroll/standby/resume", "ESP32-CAM\nĐiều khiển camera/face"),
        ("ESP32-CAM", "home/status/cam\nJSON status", "WebApp\nGiám sát camera"),
    ]
    y = 180
    for i, (pub, topic, sub) in enumerate(rows):
        style = "block_green" if i % 2 == 0 else "block"
        p = d.vertex(pub, pubs_x, y, 320, 58, style)
        t = d.vertex(topic, topic_x, y, 460, 58, "data")
        s = d.vertex(sub, subs_x, y, 470, 58, style)
        d.edge(p, t, "publish", "edge_signal")
        d.edge(t, s, "subscribe", "edge_signal")
        y += 68
    d.vertex("Lưu ý cấu hình: firmware ESP32/WebApp đang trỏ MQTT_SERVER/MQTT_URL về 192.168.88.83; Home Assistant configuration.yaml hiện đặt broker 192.168.88.150. Nếu hệ thống thực tế dùng một IP duy nhất, nên đồng bộ lại cấu hình.", 55, 1120, 1320, 42, "note")
    return d


def page_door_block() -> Diagram:
    d = Diagram("BLOCK - Khóa cửa Main", 1500, 1000)
    header(d, "Khối khóa cửa ESP32 Main", "Các nguồn xác thực đều quy về openDoor()/closeDoor(), sau đó publish trạng thái cửa qua MQTT.")
    broker = d.vertex("MQTT Broker\nhome/door/command\nhome/finger/*\nhome/keypad/*\nhome/rfid/*", 580, 115, 300, 130, "data")
    main = d.vertex("ESP32 Main\nState: isDoorOpen\nPubSubClient + Preferences", 575, 380, 310, 130, "mcu")
    sources = [
        ("Home Assistant\nlock.unlock/lock", 80, 100),
        ("WebApp\nbuttons + WebSocket", 80, 250),
        ("ESP32-CAM\nFace MATCH", 80, 400),
        ("Touch TTP223\nGPIO4", 80, 550),
        ("RFID RC522\nSPI + UID whitelist", 1110, 100),
        ("Keypad 4x4\npassword in Preferences", 1110, 250),
        ("Fingerprint DY50\nUART2 templates", 1110, 400),
        ("Gas/LCD/Buzzer\nADC + cảnh báo", 1110, 550),
    ]
    for label, x, y in sources:
        node = d.vertex(label, x, y, 260, 86, "block_green" if x < 500 else "block_yellow")
        if "Touch" in label:
            d.edge(node, main, "local toggle", "edge_signal")
        elif "Gas" in label:
            d.edge(main, node, "read/display/alert", "edge_signal")
        elif x < 500:
            d.edge(node, broker, "MQTT command", "edge_signal")
        else:
            d.edge(node, main, "local polling", "edge_signal")
    d.edge(broker, main, "subscribe commands")
    servo = d.vertex("Servo SG90/MG90S\nGPIO2 PWM\n0 deg = locked\n90 deg = unlocked", 575, 635, 310, 125, "block_green")
    state = d.vertex("home/door/state\nlocked/unlocked\nretained", 575, 810, 310, 90, "data")
    d.edge(main, servo, "servoMove()")
    d.edge(main, state, "publishState()")
    d.edge(state, broker, "feedback")
    source_note(d, "Khối này dùng mapping trong Esp32_Main/main/config.h hiện tại, không theo WiringGuide_ESP32_Main.md cũ khi có xung đột.")
    return d


def page_voice_block() -> Diagram:
    d = Diagram("BLOCK - Giọng nói", 1500, 980)
    header(d, "Khối điều khiển giọng nói", "Dự án có hai đường giọng nói: ESP32 Voice xử lý on-device và WebApp dùng Web Speech API trong trình duyệt.")
    mic = d.vertex("INMP441\nI2S0 input\n16 kHz", 80, 160, 190, 90, "block_green")
    buffer = d.vertex("ESP32 Voice\nreadAudioChunk()\naudioBuffer 1 giây", 340, 150, 240, 110, "mcu")
    vad = d.vertex("Mode mặc định: VAD test\nRMS > threshold\nZCR < 0.25\nautocorr > 0.6", 650, 115, 270, 140, "block_yellow")
    tflite = d.vertex("Mode tùy chọn: TFLite / Edge Impulse\ninference mỗi 500 ms\nconfidence >= 0.75", 650, 300, 270, 125, "block_purple")
    light = d.vertex("setLight()\nGPIO5 LED\npublish home/light/state", 1010, 175, 260, 115, "block_green")
    touch = d.vertex("TTP223 backup\nGPIO4 -> toggleLight()", 340, 350, 240, 90, "block_green")
    amp = d.vertex("MAX98357A + loa\nI2S1 output\nchuẩn bị phản hồi âm thanh", 1010, 380, 260, 105, "block")
    webspeech = d.vertex("Trình duyệt WebApp\nWeb Speech API\nmẫu lệnh: bật đèn, tắt đèn,\nmở cửa, khóa cửa", 80, 585, 285, 135, "block_purple")
    webserver = d.vertex("WebSocket server\nserver.js", 450, 610, 220, 90, "block")
    broker = d.vertex("MQTT Broker", 760, 610, 200, 90, "data")
    main = d.vertex("ESP32 Main\nnhận lệnh cửa", 1060, 590, 220, 90, "mcu")
    d.edge(mic, buffer, "I2S samples", "edge_signal")
    d.edge(buffer, vad, "test mode")
    d.edge(buffer, tflite, "AI mode")
    d.edge(vad, light, "likelySpeech -> toggle")
    d.edge(tflite, light, "YES/NO hoặc label tiếng Việt")
    d.edge(touch, light, "backup")
    d.edge(buffer, amp, "I2S1 pins đã khai báo", "edge_dash")
    d.edge(webspeech, webserver, "JSON command")
    d.edge(webserver, broker, "publish MQTT")
    d.edge(broker, light, "home/light/command\nON/OFF")
    d.edge(broker, main, "home/door/command\nUNLOCK/LOCK")
    source_note(d, "Nguồn thông tin: Esp32_Voice/Voice/Voice.ino, Esp32_Voice/Voice/config.h, WebApp/public/index.html.")
    return d


def page_camera_block() -> Diagram:
    d = Diagram("BLOCK - Camera AI", 1500, 1020)
    header(d, "Khối camera AI ESP32-CAM", "Streaming MJPEG, enroll/recognize khuôn mặt, publish kết quả qua MQTT và mở cửa khi MATCH.")
    cam = d.vertex("OV2640 camera\nAI Thinker pin mapping", 65, 155, 210, 95, "block_green")
    init = d.vertex("setup()\nSPIFFS + PSRAM check\nesp_camera_init()\nWiFi + MQTT\nstartCameraServer()", 355, 120, 260, 155, "mcu")
    server = d.vertex("HTTP camera server\n/\n/stream\n/capture\n/control\n/enroll\n/list_faces", 710, 115, 260, 165, "block_purple")
    stream = d.vertex("Stream task\ncapture frame\nskip detect mỗi 3 frame\nJPEG/RGB convert", 355, 385, 260, 135, "process")
    face = d.vertex("Face detector\nHumanFaceDetectMSR01\nbounding box", 710, 370, 260, 105, "block_yellow")
    recog = d.vertex("FaceRecognition112V1S8\ncrop 112x112\nrecognize/enroll\nSPIFFS persistent DB", 1015, 355, 295, 140, "block_yellow")
    result = d.vertex("g_face_result_pending\nMATCH / NO_MATCH / ENROLLED\ncooldown chống spam", 710, 600, 300, 115, "data")
    broker = d.vertex("MQTT Broker\nhome/door/face_result\nhome/cam/cmd\nhome/status/cam", 1015, 600, 295, 135, "data")
    main = d.vertex("ESP32 Main\nhome/door/command\nUNLOCK khi MATCH", 1015, 805, 295, 100, "mcu")
    web = d.vertex("WebApp\nstream viewer\nface log\nenroll command", 65, 600, 250, 120, "block_purple")
    d.edge(cam, init, "camera pins")
    d.edge(init, server, "start")
    d.edge(server, stream, "GET /stream")
    d.edge(stream, face, "frame")
    d.edge(face, recog, "face crop")
    d.edge(recog, result, "status + score")
    d.edge(result, broker, "mqtt_publish_face_result()")
    d.edge(broker, main, "MATCH -> UNLOCK")
    d.edge(web, server, "HTTP control/stream", "edge_dash")
    d.edge(web, broker, "home/cam/cmd", "edge_signal")
    d.edge(broker, server, "capture/enroll/standby/resume", "edge_signal")
    source_note(d, "Nguồn thông tin: Esp32_Cam/Web_server/Web_server.ino, app_httpd.cpp, mqtt_handler.cpp, config.h, camera_pins.h.")
    return d


def page_power_block() -> Diagram:
    d = Diagram("BLOCK - Nguồn cấp", 1500, 960)
    header(d, "Sơ đồ khối nguồn cấp 3.3V / 5V / 12V", "Tách nguồn theo mức điện áp; tất cả logic dùng chung GND, 12V chỉ cấp module ngoài hiển thị nhiệt độ.")
    ac = d.vertex("Adapter / nguồn DC", 60, 160, 180, 80, "block")
    five = d.vertex("Bus 5V\nkhuyến nghị 5V/2A hoặc hơn", 330, 110, 240, 90, "rail5")
    three = d.vertex("Bus 3.3V\nLDO onboard ESP32\ncho module logic nhạy", 330, 290, 240, 90, "rail3")
    twelve = d.vertex("Bus 12V riêng", 330, 470, 240, 80, "rail12")
    gnd = d.vertex("GND chung\nstar/rail ground", 330, 650, 240, 80, "railg")
    d.edge(ac, five, "5V", "edge_power5")
    d.edge(five, three, "LDO ESP32", "edge_power3")
    d.edge(ac, twelve, "12V adapter riêng", "edge_power12")
    loads5 = [
        ("ESP32 DevKit Main/Voice\nVIN/USB", 700, 80),
        ("ESP32-CAM AI Thinker\n5V 2A riêng", 700, 190),
        ("Servo SG90/MG90S\nstall cao, nên tách nhánh", 700, 300),
        ("MAX98357A amp\n100uF + 470uF sát VIN", 700, 410),
        ("LCD I2C / MQ sensor / buzzer\nnếu module yêu cầu 5V", 700, 520),
    ]
    loads3 = [
        ("RC522 RFID\n3.3V bắt buộc", 1070, 120),
        ("INMP441 mic\n3.3V bắt buộc", 1070, 230),
        ("TTP223 / logic UART/I2C\nưu tiên 3.3V signal", 1070, 340),
    ]
    for label, x, y in loads5:
        n = d.vertex(label, x, y, 270, 75, "block_red")
        d.edge(five, n, "", "edge_power5")
        d.edge(gnd, n, "GND", "edge_gnd")
    for label, x, y in loads3:
        n = d.vertex(label, x, y, 270, 75, "block_green")
        d.edge(three, n, "", "edge_power3")
        d.edge(gnd, n, "GND", "edge_gnd")
    temp = d.vertex("Module ngoài hiển thị nhiệt độ\nNguồn 12V + GND\nKhông nối GPIO ESP32", 1070, 560, 300, 105, "rail12")
    d.edge(twelve, temp, "12V", "edge_power12")
    d.edge(gnd, temp, "GND", "edge_gnd")
    d.vertex("Khuyến nghị kỹ thuật\n- Servo, ESP32-CAM và amplifier có dòng đỉnh cao: tách nhánh 5V, tụ bulk gần tải.\n- Nếu LCD/MQ dùng 5V nhưng tín hiệu vào ESP32, cần bảo đảm mức logic/ADC không vượt 3.3V.\n- GND phải nối chung giữa nguồn ngoài và các ESP32 để tín hiệu có tham chiếu.", 60, 785, 1300, 105, "note")
    return d


def page_pin_main() -> Diagram:
    d = Diagram("PIN - ESP32 Main", 1700, 1260)
    header(d, "Sơ đồ chân ESP32 Main", "Theo firmware hiện tại trong Esp32_Main/main/config.h; có vẽ bus 3.3V, 5V, 12V và GND chung.")
    mcu = d.vertex("ESP32 DevKit V1 - MAIN\nWiFi + MQTT\nDoor/Gas/RFID/Keypad/Fingerprint\n\nPin chính:\nGPIO2 Servo PWM\nGPIO4 TTP223 OUT\nGPIO5 RC522 SS\nGPIO12 Buzzer\nGPIO13/15/14/26 Keypad rows\nGPIO25/33/32/21 Keypad cols\nGPIO16 RX2, GPIO17 TX2 Fingerprint\nGPIO18/19/23 SPI\nGPIO22 SCL, GPIO0 SDA\nGPIO27 RFID RST\nGPIO34 Gas ADC", 620, 185, 380, 470, "mcu")
    rail3 = d.vertex("3.3V rail", 80, 110, 190, 55, "rail3")
    rail5 = d.vertex("5V rail", 310, 110, 190, 55, "rail5")
    rail12 = d.vertex("12V rail", 540, 110, 190, 55, "rail12")
    gnd = d.vertex("GND chung", 770, 110, 190, 55, "railg")
    rfid = d.vertex("RC522 RFID\nVCC 3.3V, GND\nSCK GPIO18\nMOSI GPIO23\nMISO GPIO19\nSS/SDA GPIO5\nRST GPIO27", 70, 230, 290, 155, "block_green")
    keypad = d.vertex("Keypad 4x4\nR1 GPIO13\nR2 GPIO15\nR3 GPIO14\nR4 GPIO26\nC1 GPIO25\nC2 GPIO33\nC3 GPIO32\nC4 GPIO21", 70, 440, 290, 185, "block_green")
    touch = d.vertex("TTP223 Touch\nVCC 3.3V\nOUT GPIO4", 70, 695, 250, 90, "block_green")
    fp = d.vertex("Fingerprint DY50\nUART2\nDY50 TX -> GPIO16 RX2\nDY50 RX <- GPIO17 TX2\nVCC theo module", 70, 840, 310, 120, "block_yellow")
    servo = d.vertex("Servo SG90/MG90S\n5V + GND\nSIG GPIO2 PWM\n0 deg locked, 90 deg unlocked", 1160, 230, 300, 125, "block_red")
    gas = d.vertex("MQ Gas Sensor\nVCC 5V, GND\nAO -> GPIO34 ADC\nNếu AO 0-5V: cần chia áp về 3.3V", 1160, 410, 330, 125, "block_red")
    lcd = d.vertex("LCD I2C 16x2\nVCC 5V hoặc 3.3V\nSDA GPIO0\nSCL GPIO22\nPull-up không vượt 3.3V", 1160, 590, 330, 125, "block_yellow")
    buzz = d.vertex("Active Buzzer\nGPIO12\nGND\nNếu dùng 5V nên qua transistor", 1160, 770, 300, 105, "block_red")
    temp = d.vertex("Module ngoài hiển thị nhiệt độ\nNguồn 12V + GND\nKhông nối GPIO ESP32", 1160, 930, 330, 105, "rail12")
    for node in [rfid, touch]:
        d.edge(rail3, node, "", "edge_power3")
    for node in [servo, gas, lcd, buzz]:
        d.edge(rail5, node, "", "edge_power5")
    d.edge(rail12, temp, "", "edge_power12")
    for node in [rfid, keypad, touch, fp, servo, gas, lcd, buzz, temp]:
        d.edge(gnd, node, "", "edge_gnd")
    d.edge(mcu, rfid, "SPI: 18/23/19 + SS5 + RST27", "edge_signal")
    d.edge(mcu, keypad, "8 GPIO matrix", "edge_signal")
    d.edge(touch, mcu, "OUT GPIO4", "edge_signal")
    d.edge(fp, mcu, "UART2 16/17", "edge_signal")
    d.edge(mcu, servo, "GPIO2 PWM", "edge_signal")
    d.edge(gas, mcu, "ADC GPIO34", "edge_signal")
    d.edge(mcu, lcd, "I2C SDA0/SCL22", "edge_signal")
    d.edge(mcu, buzz, "GPIO12", "edge_signal")
    d.vertex("Cảnh báo boot/điện áp\n- GPIO0 là strapping pin: giữ pull-up HIGH khi boot.\n- RC522 dùng 3.3V, không cấp 5V.\n- MQ AO không được vượt 3.3V vào ADC ESP32.\n- 12V chỉ cấp module hiển thị nhiệt độ độc lập.", 70, 1070, 1420, 90, "note")
    return d


def page_pin_voice() -> Diagram:
    d = Diagram("PIN - ESP32 Voice", 1650, 1120)
    header(d, "Sơ đồ chân ESP32 Voice", "Theo Esp32_Voice/Voice/config.h hiện tại: INMP441 I2S input, MAX98357A I2S output, LED GPIO5, TTP223 GPIO4.")
    mcu = d.vertex("ESP32 DevKit V1 - VOICE\nWiFi + MQTT + I2S\n\nGPIO14 I2S0 BCLK mic\nGPIO15 I2S0 WS mic\nGPIO32 I2S0 DIN mic\nGPIO26 I2S1 BCLK speaker\nGPIO25 I2S1 WS/LRC speaker\nGPIO27 I2S1 DOUT speaker\nGPIO5 LED output\nGPIO4 TTP223 touch", 610, 210, 380, 390, "mcu")
    rail3 = d.vertex("3.3V rail", 70, 110, 190, 55, "rail3")
    rail5 = d.vertex("5V rail", 300, 110, 190, 55, "rail5")
    rail12 = d.vertex("12V rail", 530, 110, 190, 55, "rail12")
    gnd = d.vertex("GND chung", 760, 110, 190, 55, "railg")
    mic = d.vertex("INMP441 Mic\nVDD 3.3V\nGND + L/R -> GND\nSCK GPIO14\nWS GPIO15\nSD GPIO32", 80, 255, 300, 145, "block_green")
    touch = d.vertex("TTP223 Touch\nVCC 3.3V\nOUT GPIO4", 80, 465, 250, 90, "block_green")
    led = d.vertex("LED / đèn demo\nGPIO5 -> LED/driver -> GND\nState ON/OFF publish MQTT", 80, 635, 300, 105, "block_green")
    amp = d.vertex("MAX98357A I2S Amplifier\nVIN 5V + GND\nBCLK GPIO26\nLRC GPIO25\nDIN GPIO27\nOut+ / Out- -> loa\n100uF + 470uF sát VIN", 1120, 255, 330, 180, "block_red")
    speaker = d.vertex("Loa 4 ohm / 8 ohm\nBTL output\nkhông nối Out- xuống GND", 1120, 510, 300, 90, "block")
    temp = d.vertex("Module ngoài hiển thị nhiệt độ\nNguồn 12V + GND\nKhông nối GPIO ESP32", 1120, 685, 330, 105, "rail12")
    for node in [mic, touch]:
        d.edge(rail3, node, "", "edge_power3")
    d.edge(rail5, amp, "", "edge_power5")
    d.edge(rail12, temp, "", "edge_power12")
    for node in [mic, touch, led, amp, temp]:
        d.edge(gnd, node, "", "edge_gnd")
    d.edge(mcu, mic, "I2S0 RX: 14/15/32", "edge_signal")
    d.edge(touch, mcu, "GPIO4", "edge_signal")
    d.edge(mcu, led, "GPIO5", "edge_signal")
    d.edge(mcu, amp, "I2S1 TX: 26/25/27", "edge_signal")
    d.edge(amp, speaker, "Out+ / Out-", "edge_signal")
    d.vertex("Nguồn/âm thanh\nMAX98357A tạo dòng xung khi phát loa, nên dùng nhánh 5V đủ dòng và tụ lọc gần module. INMP441 chỉ cấp 3.3V.", 80, 850, 1320, 90, "note")
    return d


def page_pin_cam() -> Diagram:
    d = Diagram("PIN - ESP32-CAM", 1650, 1180)
    header(d, "Sơ đồ chân ESP32-CAM AI Thinker", "Theo camera_pins.h và config.h: phần lớn GPIO được OV2640 dùng nội bộ; cấp nguồn 5V/2A riêng cho camera.")
    board = d.vertex("ESP32-CAM AI Thinker\nESP32 + OV2640 + PSRAM\n\nCamera internal pins:\nD0 GPIO5, D1 GPIO18, D2 GPIO19, D3 GPIO21\nD4 GPIO36, D5 GPIO39, D6 GPIO34, D7 GPIO35\nXCLK GPIO0, PCLK GPIO22\nVSYNC GPIO25, HREF GPIO23\nSIOD GPIO26, SIOC GPIO27\nPWDN GPIO32, RESET NC\nFlash LED GPIO4\nStatus LED GPIO33 active LOW", 565, 205, 460, 500, "mcu")
    rail5 = d.vertex("5V rail\nkhuyến nghị 5V/2A", 90, 110, 240, 60, "rail5")
    rail3 = d.vertex("3.3V onboard\nESP32-CAM LDO", 360, 110, 240, 60, "rail3")
    rail12 = d.vertex("12V rail", 630, 110, 210, 60, "rail12")
    gnd = d.vertex("GND chung", 870, 110, 210, 60, "railg")
    ov = d.vertex("OV2640 camera\nkết nối nội bộ qua ribbon\nQVGA 320x240 JPEG", 70, 270, 310, 110, "block_green")
    psram = d.vertex("PSRAM\nbắt buộc Enabled\nframe buffer + face AI", 70, 450, 280, 95, "block_purple")
    leds = d.vertex("LED onboard\nFlash trắng GPIO4 active HIGH\nLED đỏ GPIO33 active LOW", 70, 615, 320, 105, "block_yellow")
    wifi = d.vertex("WiFi + MQTT\nhome/cam/cmd\nhome/door/face_result\nhome/status/cam", 1120, 260, 350, 120, "data")
    http = d.vertex("HTTP endpoints\n/stream, /capture,\n/control, /enroll,\n/list_faces, /face_result", 1120, 455, 350, 135, "block_purple")
    temp = d.vertex("Module ngoài hiển thị nhiệt độ\nNguồn 12V + GND\nKhông nối GPIO ESP32-CAM", 1120, 690, 350, 105, "rail12")
    d.edge(rail5, board, "", "edge_power5")
    d.edge(board, rail3, "LDO 3.3V onboard", "edge_power3")
    d.edge(rail12, temp, "", "edge_power12")
    for node in [board, ov, psram, leds, temp]:
        d.edge(gnd, node, "", "edge_gnd")
    d.edge(board, ov, "camera pins", "edge_signal")
    d.edge(board, psram, "memory bus nội bộ", "edge_signal")
    d.edge(board, leds, "GPIO4/GPIO33", "edge_signal")
    d.edge(board, wifi, "MQTT", "edge_signal")
    d.edge(board, http, "Web server", "edge_signal")
    d.vertex("GPIO còn trống rất hạn chế: GPIO12/13/14/15 có thể dùng nhưng thường chia sẻ SD card và có ràng buộc boot. Không nên gắn thêm tải 12V vào board camera.", 70, 850, 1370, 90, "note")
    return d


def page_flow_main_loop() -> Diagram:
    d = Diagram("FLOW - Main loop", 1400, 1100)
    header(d, "Lưu đồ thuật toán ESP32 Main loop", "Vòng lặp chính xử lý MQTT, cảm biến/đầu vào khóa cửa, gas/LCD/buzzer và vân tay.")
    ids = vertical_flow(
        d,
        520,
        120,
        [
            ("Bắt đầu setup()", "start", 70),
            ("Khởi tạo Serial, LCD I2C, Buzzer, MQ gas, Touch", "process", 80),
            ("Servo về góc khóa 0 deg\nWiFi connect\nMQTT setServer + subscribe", "process", 95),
            ("Khởi tạo RFID SPI + Preferences\nKhởi tạo DY50 Fingerprint UART2", "process", 95),
            ("loop(): lấy now = millis()", "process", 70),
            ("MQTT còn kết nối?", "decision", 105),
            ("mqtt.loop()", "process", 65),
            ("Poll Touch, RFID, Keypad", "process", 75),
            ("Đủ 1000 ms cập nhật Gas + LCD?", "decision", 110),
            ("updateBuzzer() liên tục", "process", 65),
            ("Đang enroll fingerprint?", "decision", 110),
            ("AUTO_LOCK_MS > 0 và cửa đang mở quá thời gian?", "decision", 115),
            ("Quay lại đầu loop", "end", 65),
        ],
        gap=16,
        w=360,
    )
    reconnect = d.vertex("Nếu không kết nối và quá MQTT_RETRY_MS:\nmqttConnect()", 935, 520, 280, 80, "process2")
    lcd = d.vertex("updateLCD(): warmup hoặc đọc ADC,\npublish home/gas, in LCD", 935, 735, 310, 85, "process2")
    fp_enroll = d.vertex("fpHandleEnroll()", 145, 840, 230, 65, "process2")
    fp_check = d.vertex("Nếu idle và đủ 100 ms:\nfpCheckFinger()", 145, 930, 250, 75, "process2")
    autolock = d.vertex("closeDoor()", 935, 955, 220, 60, "process2")
    d.edge(ids[5], reconnect, "Không", "edge")
    d.edge(reconnect, ids[6], "")
    d.edge(ids[8], lcd, "Có", "edge")
    d.edge(lcd, ids[9], "")
    d.edge(ids[10], fp_enroll, "Có", "edge")
    d.edge(ids[10], fp_check, "Không", "edge")
    d.edge(fp_enroll, ids[11], "")
    d.edge(fp_check, ids[11], "")
    d.edge(ids[11], autolock, "Có", "edge")
    d.edge(autolock, ids[12], "")
    source_note(d, "AUTO_LOCK_MS hiện bằng 0 trong config.h, nên nhánh tự khóa bị tắt ở firmware hiện tại.")
    return d


def page_flow_door() -> Diagram:
    d = Diagram("FLOW - Điều khiển cửa", 1400, 900)
    header(d, "Lưu đồ thuật toán điều khiển cửa", "Mọi nguồn mở/khóa đều gọi openDoor()/closeDoor(), servo chạy xong sẽ detach để giảm rung.")
    src = d.vertex("Nguồn lệnh\nMQTT UNLOCK/LOCK\nTouch toggle\nRFID hợp lệ\nKeypad đúng\nVân tay khớp\nFace MATCH", 65, 190, 255, 175, "io")
    cmd = d.vertex("Xác định yêu cầu\nMở cửa hay khóa cửa?", 430, 205, 245, 120, "decision")
    chk_open = d.vertex("Yêu cầu mở\nisDoorOpen == false?", 790, 110, 230, 100, "decision")
    open_d = d.vertex("openDoor()\nisDoorOpen = true\nunlockStart = millis()", 1080, 110, 230, 105, "process")
    chk_close = d.vertex("Yêu cầu khóa\nisDoorOpen == true?", 790, 315, 230, 100, "decision")
    close_d = d.vertex("closeDoor()\nisDoorOpen = false", 1080, 325, 230, 80, "process")
    servo = d.vertex("servoMove(angle)\nattach GPIO2\nwrite 90/0 deg\ndelay 600 ms\ndetach", 540, 515, 275, 135, "process2")
    state = d.vertex("publishState()\nhome/door/state\nunlocked/locked retained", 900, 535, 290, 95, "data")
    done = d.vertex("Kết thúc nhánh", 615, 735, 190, 60, "end")
    d.edge(src, cmd)
    d.edge(cmd, chk_open, "Mở")
    d.edge(cmd, chk_close, "Khóa")
    d.edge(chk_open, open_d, "Có")
    d.edge(chk_close, close_d, "Có")
    d.edge(open_d, servo)
    d.edge(close_d, servo)
    d.edge(servo, state)
    d.edge(state, done)
    d.vertex("Nếu trạng thái yêu cầu trùng trạng thái hiện tại, code bỏ qua để không chạy servo thừa.", 80, 690, 390, 80, "note")
    return d


def page_flow_rfid() -> Diagram:
    d = Diagram("FLOW - RFID", 1400, 980)
    header(d, "Lưu đồ thuật toán RFID RC522", "Quét UID, thêm thẻ theo lệnh add_next hoặc kiểm tra whitelist trong Preferences.")
    scan = d.vertex("rfidCheck()", 575, 115, 220, 60, "start")
    present = d.vertex("Có thẻ mới và đọc serial OK?", 545, 215, 280, 110, "decision")
    uid = d.vertex("rfidGetUID()\nFormat HEX có dấu ':'", 560, 375, 250, 75, "process")
    addnext = d.vertex("rfidAddNext == true?", 545, 500, 280, 105, "decision")
    add = d.vertex("rfidAddCard(uid)\nNếu chưa có và count < 20\npublish added/already", 170, 635, 290, 115, "process2")
    allowed = d.vertex("uid nằm trong whitelist?", 860, 635, 280, 105, "decision")
    ok = d.vertex("publish ok:UID\nNếu cửa đang khóa -> openDoor()", 805, 790, 310, 85, "process2")
    denied = d.vertex("publish denied:UID", 1145, 790, 190, 65, "process2")
    halt = d.vertex("PICC_HaltA()\nPCD_StopCrypto1()", 545, 895, 280, 65, "end")
    d.edge(scan, present)
    d.edge(present, uid, "Có")
    d.edge(uid, addnext)
    d.edge(addnext, add, "Có")
    d.edge(addnext, allowed, "Không")
    d.edge(allowed, ok, "Có")
    d.edge(allowed, denied, "Không")
    d.edge(add, halt)
    d.edge(ok, halt)
    d.edge(denied, halt)
    d.vertex("Nếu không có thẻ mới hoặc đọc serial thất bại thì return ngay.", 100, 240, 300, 75, "note")
    return d


def page_flow_keypad() -> Diagram:
    d = Diagram("FLOW - Keypad", 1400, 980)
    header(d, "Lưu đồ thuật toán Keypad 4x4", "Nhận phím, lưu buffer số, '#' xác nhận mật khẩu, '*' xóa buffer.")
    start = d.vertex("handleKeypad()", 580, 115, 220, 60, "start")
    key = d.vertex("keypad.getKey()", 565, 220, 250, 70, "io")
    has = d.vertex("Có phím?", 585, 335, 210, 90, "decision")
    star = d.vertex("key == '*' ?", 560, 475, 250, 90, "decision")
    clear = d.vertex("kpBuffer = \"\"\npublish clear", 180, 590, 220, 75, "process2")
    hashd = d.vertex("key == '#' ?", 560, 610, 250, 90, "decision")
    compare = d.vertex("So sánh kpBuffer\nvới kpGetPassword()", 540, 750, 290, 85, "process")
    ok = d.vertex("Đúng:\npublish ok\nopenDoor()", 885, 725, 220, 90, "process2")
    wrong = d.vertex("Sai:\npublish wrong", 885, 840, 220, 70, "process2")
    digit = d.vertex("Nếu là số 0..9 và độ dài < KP_MAX_LEN\nappend vào kpBuffer", 960, 560, 300, 95, "process2")
    end = d.vertex("Return", 570, 910, 230, 55, "end")
    d.edge(start, key)
    d.edge(key, has)
    d.edge(has, star, "Có")
    d.edge(has, end, "Không")
    d.edge(star, clear, "Có")
    d.edge(star, hashd, "Không")
    d.edge(clear, end)
    d.edge(hashd, compare, "Có")
    d.edge(hashd, digit, "Không")
    d.edge(compare, ok, "Bằng")
    d.edge(compare, wrong, "Khác")
    d.edge(ok, end)
    d.edge(wrong, end)
    d.edge(digit, end)
    d.vertex("Mật khẩu mặc định KP_DEFAULT_PASS = 1006; WebApp đổi mật khẩu bằng topic home/keypad/cmd payload setpass:xxxx.", 80, 830, 360, 80, "note")
    return d


def page_flow_fingerprint() -> Diagram:
    d = Diagram("FLOW - Fingerprint", 1500, 1080)
    header(d, "Lưu đồ thuật toán vân tay DY50", "Gồm hai luồng: enroll bằng MQTT command và nhận diện định kỳ khi ở trạng thái idle.")
    enroll_cmd = d.vertex("MQTT home/finger/enroll\npayload ID", 60, 160, 270, 80, "io")
    valid = d.vertex("ID 1..127 và enrollState == IDLE?", 410, 145, 290, 105, "decision")
    start = d.vertex("fpEnrollStart(id)\npublish place_finger\nstate = WAIT_FIRST", 790, 150, 300, 95, "process")
    first = d.vertex("WAIT_FIRST\ngetImage OK?\nimage2Tz(1) OK?", 790, 305, 300, 105, "decision")
    remove = d.vertex("WAIT_REMOVE\nNOFINGER?\npublish place_again", 790, 465, 300, 100, "decision")
    second = d.vertex("WAIT_SECOND\ngetImage OK?\nimage2Tz(2) OK?", 790, 620, 300, 105, "decision")
    model = d.vertex("createModel()\nstoreModel(enrollId)", 790, 785, 300, 85, "process")
    ok = d.vertex("publish ok:id\nstate = IDLE", 1115, 800, 230, 70, "end")
    fail = d.vertex("Bất kỳ lỗi/timeout:\npublish fail:*\nstate = IDLE", 410, 820, 270, 90, "end")
    idle = d.vertex("Loop idle mỗi 100 ms\nfpCheckFinger()", 60, 520, 270, 80, "start")
    search = d.vertex("getImage + image2Tz + fingerSearch OK?", 410, 505, 290, 105, "decision")
    open_d = d.vertex("Khớp ID:\nopenDoor() nếu cửa đang khóa", 410, 650, 290, 80, "process2")
    d.edge(enroll_cmd, valid)
    d.edge(valid, start, "Có")
    d.edge(valid, fail, "Không")
    d.edge(start, first)
    d.edge(first, remove, "OK")
    d.edge(first, fail, "Lỗi/timeout")
    d.edge(remove, second, "Đã nhấc tay")
    d.edge(remove, fail, "Timeout")
    d.edge(second, model, "OK")
    d.edge(second, fail, "Lỗi/timeout")
    d.edge(model, ok, "OK")
    d.edge(model, fail, "Store lỗi")
    d.edge(idle, search)
    d.edge(search, open_d, "OK")
    d.edge(search, idle, "Không")
    d.vertex("MQTT delete: home/finger/delete payload ID -> finger.deleteModel(id) -> publish deleted:id hoặc fail:delete.", 60, 930, 1280, 65, "note")
    return d


def page_flow_gas() -> Diagram:
    d = Diagram("FLOW - Gas LCD Buzzer", 1400, 980)
    header(d, "Lưu đồ thuật toán Gas + LCD + Buzzer", "MQ sensor warmup 30 giây, đọc ADC mỗi 1 giây, hiển thị LCD và cảnh báo buzzer.")
    start = d.vertex("Mỗi 1000 ms trong loop\nupdateLCD()", 560, 115, 280, 70, "start")
    warm = d.vertex("millis() - setupTime < WARMUP_MS?", 530, 245, 340, 105, "decision")
    warm_lcd = d.vertex("LCD:\nKhoi dong sensor\nWarmup: Ns\nKhông publish gas", 170, 405, 270, 105, "process2")
    read = d.vertex("raw = analogRead(GPIO34)\nlastGasRaw = raw\npublish home/gas retained", 540, 405, 320, 105, "process")
    level = d.vertex("So sánh ngưỡng\nDANGER >= 2500\nWARN >= 1500", 560, 560, 280, 105, "decision")
    normal = d.vertex("LCD: Binh thuong\nBuzzer LOW", 170, 720, 250, 75, "block_green")
    warn = d.vertex("LCD: CANH BAO\nBuzzer beep mỗi 500 ms", 560, 720, 270, 75, "block_yellow")
    danger = d.vertex("LCD: NGUY HIEM\nBuzzer HIGH liên tục", 960, 720, 270, 75, "block_red")
    done = d.vertex("Quay lại loop", 570, 880, 250, 55, "end")
    d.edge(start, warm)
    d.edge(warm, warm_lcd, "Có")
    d.edge(warm, read, "Không")
    d.edge(read, level)
    d.edge(level, normal, "raw < 1500")
    d.edge(level, warn, "1500..2499")
    d.edge(level, danger, ">= 2500")
    d.edge(warm_lcd, done)
    d.edge(normal, done)
    d.edge(warn, done)
    d.edge(danger, done)
    d.vertex("updateBuzzer() chạy liên tục ngoài chu kỳ LCD để beep chính xác; warmup luôn tắt buzzer.", 80, 845, 350, 75, "note")
    return d


def page_flow_voice_vad() -> Diagram:
    d = Diagram("FLOW - Voice VAD", 1500, 1080)
    header(d, "Lưu đồ thuật toán Voice/VAD", "Mode mặc định không bật USE_TFLITE_MODEL/USE_EDGE_IMPULSE: phát hiện giọng nói bằng RMS, ZCR và autocorrelation.")
    ids = vertical_flow(
        d,
        560,
        115,
        [
            ("setup()\nTắt brownout, Serial, GPIO5 LED, GPIO4 Touch", "start", 80),
            ("WiFi + MQTT\nsubscribe home/light/command\npublish trạng thái ban đầu", "process", 95),
            ("setupI2S()\nINMP441 I2S0 RX\n16 kHz, 32-bit frame", "process", 90),
            ("loop()\nMQTT keep-alive\nhandleTouch()", "process", 80),
            ("readAudioChunk()\nGhi ring buffer 1 giây", "io", 80),
            ("Mode compile là gì?", "decision", 110),
        ],
        gap=22,
        w=360,
    )
    vad = d.vertex("VAD test mode\ncomputeRMS(chunk)", 180, 635, 260, 80, "process2")
    thr = d.vertex("RMS > VAD_RMS_THRESHOLD\nvà qua cooldown?", 170, 760, 285, 100, "decision")
    speech = d.vertex("Nói đủ VAD_SPEECH_MIN_MS\nZCR < 0.25\nautocorr > 0.6", 160, 905, 310, 105, "decision")
    toggle = d.vertex("toggleLight()\nlastCommandTime = now\nwaitingForSilence = true", 540, 890, 310, 105, "process2")
    ai = d.vertex("TFLite / Edge Impulse mode\nmỗi 500 ms sau khi đủ 1 giây audio", 955, 635, 330, 90, "block_purple")
    infer = d.vertex("run inference\nconfidence >= 0.75?", 980, 765, 280, 95, "decision")
    setlight = d.vertex("YES/bat_den -> setLight(true)\nNO/tat_den -> setLight(false)", 945, 900, 350, 85, "process2")
    status = d.vertex("publishLightState()\nhome/light/state", 575, 1015, 280, 60, "data")
    d.edge(ids[-1], vad, "Không define model")
    d.edge(vad, thr)
    d.edge(thr, speech, "Có")
    d.edge(speech, toggle, "likelySpeech")
    d.edge(toggle, status)
    d.edge(ids[-1], ai, "USE_TFLITE_MODEL hoặc USE_EDGE_IMPULSE")
    d.edge(ai, infer)
    d.edge(infer, setlight, "Có")
    d.edge(setlight, status)
    d.vertex("Nếu đang cooldown hoặc chưa im lặng lại thì bỏ qua để tránh toggle lặp liên tục.", 90, 510, 380, 80, "note")
    return d


def page_flow_cam_face() -> Diagram:
    d = Diagram("FLOW - CAM Face", 1500, 1080)
    header(d, "Lưu đồ thuật toán ESP32-CAM Face Recognition", "Luồng frame từ /stream xử lý detect/recognize/enroll và chuyển kết quả sang main loop để publish MQTT.")
    setup = d.vertex("setup()\nSPIFFS, PSRAM, camera,\nWiFi, MQTT, startCameraServer()", 520, 115, 380, 110, "start")
    stream = d.vertex("Client mở /stream\nesp_camera_fb_get()", 540, 290, 340, 85, "io")
    detect_on = d.vertex("g_face_detect_on và frame skip đúng?", 535, 430, 350, 110, "decision")
    detect = d.vertex("Face detection\nconvert frame -> RGB\nbounding boxes", 185, 595, 300, 105, "process")
    enroll = d.vertex("g_is_enrolling?", 570, 595, 260, 95, "decision")
    enroll_do = d.vertex("enroll_id(face_tensor, name)\nstatus ENROLLED\nsave DB", 915, 575, 300, 105, "process2")
    recog_on = d.vertex("g_face_recognize_on?", 560, 750, 280, 95, "decision")
    recognize = d.vertex("recognize(face_tensor)\nscore/similarity", 900, 735, 300, 90, "process2")
    match = d.vertex("score >= threshold?", 900, 875, 290, 95, "decision")
    match_do = d.vertex("MATCH:\nset pending result\nmqtt_door_unlock()\napply cooldown", 1210, 820, 230, 115, "block_green")
    nomatch_do = d.vertex("NO_MATCH:\nset pending result\napply cooldown", 1210, 955, 230, 85, "block_red")
    publish = d.vertex("loop()\nif g_face_result_pending:\nmqtt_publish_face_result()", 165, 840, 315, 105, "data")
    d.edge(setup, stream)
    d.edge(stream, detect_on)
    d.edge(detect_on, detect, "Có")
    d.edge(detect, enroll)
    d.edge(enroll, enroll_do, "Có")
    d.edge(enroll, recog_on, "Không")
    d.edge(enroll_do, publish)
    d.edge(recog_on, recognize, "Có")
    d.edge(recognize, match)
    d.edge(match, match_do, "Có")
    d.edge(match, nomatch_do, "Không")
    d.edge(match_do, publish)
    d.edge(nomatch_do, publish)
    d.vertex("Nếu detection tắt, stream vẫn trả MJPEG nhưng không chạy AI. WebApp có thể bật/tắt detect/recognize qua /control hoặc home/cam/cmd.", 80, 260, 350, 90, "note")
    return d


def page_flow_webapp() -> Diagram:
    d = Diagram("FLOW - WebApp", 1500, 1020)
    header(d, "Lưu đồ thuật toán WebApp MQTT bridge", "server.js nối Browser/WebSocket với MQTT Broker, broadcast trạng thái realtime và chuyển lệnh điều khiển.")
    start = d.vertex("Khởi động server.js", 575, 115, 250, 60, "start")
    express = d.vertex("Express static public/\nHTTP 3000\nHTTPS 3443 nếu có cert", 520, 220, 360, 90, "process")
    mqtt = d.vertex("mqtt.connect(MQTT_URL)\nsubscribe state/light/fp/rfid/keypad/gas/face/cam", 505, 360, 390, 105, "data")
    ws = d.vertex("Browser WebSocket connected\nGửi snapshot state hiện tại", 520, 515, 360, 90, "process")
    msg = d.vertex("Nhận WebSocket message?", 540, 655, 320, 105, "decision")
    publish = d.vertex("Map cmd -> MQTT publish\nunlock/lock/light_on/light_off\nkp/rfid/fp/cam commands", 120, 795, 380, 120, "process2")
    mqtt_msg = d.vertex("Nhận MQTT message?", 960, 655, 320, 105, "decision")
    update = d.vertex("Cập nhật state/log\nbroadcast JSON cho mọi browser", 930, 800, 360, 95, "process2")
    face = d.vertex("Nếu topic face_result\nvà status MATCH:\npublish home/door/command UNLOCK", 555, 820, 330, 105, "block_green")
    d.edge(start, express)
    d.edge(express, mqtt)
    d.edge(mqtt, ws)
    d.edge(ws, msg)
    d.edge(msg, publish, "Có")
    d.edge(ws, mqtt_msg)
    d.edge(mqtt_msg, update, "Có")
    d.edge(update, face, "nếu FACE MATCH")
    d.edge(face, mqtt)
    d.edge(publish, mqtt)
    d.vertex("Browser-side voice recognition nằm trong public/index.html: nhận câu nói, khớp pattern, gửi WebSocket cmd cho server.js.", 80, 570, 390, 75, "note")
    return d


def build() -> ET.Element:
    mxfile = ET.Element(
        "mxfile",
        {
            "host": "Electron",
            "agent": "draw.io/29.6.6 generated by tools/generate_smarthome_drawio.py",
            "version": "29.6.6",
            "modified": datetime.now().isoformat(timespec="seconds"),
            "type": "device",
        },
    )
    pages = [
        page_overview(),
        page_mqtt_topics(),
        page_door_block(),
        page_voice_block(),
        page_camera_block(),
        page_power_block(),
        page_pin_main(),
        page_pin_voice(),
        page_pin_cam(),
        page_flow_main_loop(),
        page_flow_door(),
        page_flow_rfid(),
        page_flow_keypad(),
        page_flow_fingerprint(),
        page_flow_gas(),
        page_flow_voice_vad(),
        page_flow_cam_face(),
        page_flow_webapp(),
    ]
    for p in pages:
        diag = ET.SubElement(mxfile, "diagram", {"name": p.name, "id": str(uuid.uuid4())})
        diag.append(p.model)
    return mxfile


def main() -> None:
    xml = ET.tostring(build(), encoding="utf-8")
    pretty = minidom.parseString(xml).toprettyxml(indent="  ", encoding="utf-8")
    OUT.write_bytes(pretty)
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
