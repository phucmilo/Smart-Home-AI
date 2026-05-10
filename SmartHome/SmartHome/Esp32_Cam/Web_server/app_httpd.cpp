// ============================================
// app_httpd.cpp – Web Server + Face Detection/Recognition
// ESP32-CAM AI Thinker – Smart Home VKU v2.0
//
// Dựa trên CameraWebServer example (ESP32 Core 2.0.x)
// Tùy chỉnh: MQTT publish, SPIFFS face DB, Face Recognition
//
// ĐÒI HỎI: Arduino ESP32 Board Core version 2.0.0 ~ 2.0.4
//           (các version sau đã gỡ bỏ Face Recognition cho ESP32 gốc)
// ============================================

#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"
#include <WiFi.h>
#include "config.h"
#include "mqtt_handler.h"

// ===== Face Detection & Recognition – esp-dl API (Core 2.0.4) =====
// MSR01 cung cấp facial keypoints (cần để enroll/recognize hoạt động)
// S8 giữ để giảm kích thước binary
#include "human_face_detect_msr01.hpp"
#include "face_recognition_112_v1_s8.hpp"
#define HAS_FACE_DETECT 1
#define HAS_FACE_RECOG  1

static HumanFaceDetectMSR01  *s_detector   = nullptr;
FaceRecognition112V1S8       *s_recognizer = nullptr;  // extern từ Web_server.ino

// ==========================================
// GLOBAL STATE
// ==========================================
bool g_face_detect_on   = false;
bool g_face_recognize_on = false;
bool g_is_enrolling     = false;
char g_enroll_name[FACE_NAME_MAX] = "";

// MQTT publish throttle (tránh spam)
static unsigned long g_last_match_time    = 0;
static unsigned long g_last_nomatch_time  = 0;
static int           g_last_match_id      = -1;

// Cross-task face result (stream task → main loop)
volatile bool   g_face_result_pending = false;
volatile float  g_face_result_score   = 0;
char            g_face_result_name[FACE_NAME_MAX] = "";
char            g_face_result_status[16] = "";

// Kết quả mới nhất để web poll
static unsigned long g_last_face_time = 0;
static float         g_last_face_score = 0;
static char          g_last_face_name[FACE_NAME_MAX] = "";
static char          g_last_face_status[16] = "";

// ==========================================
// MJPEG STREAMING CONSTANTS
// ==========================================
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CT   = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUND = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d.%06d\r\n\r\n";

// HTTP server handles
static httpd_handle_t camera_httpd = NULL;
static httpd_handle_t stream_httpd = NULL;

// ==========================================
// HELPER: Draw rectangle on RGB888 buffer
// ==========================================
static void draw_rect_rgb888(uint8_t* buf, int width, int height,
                              int x1, int y1, int x2, int y2,
                              uint8_t r, uint8_t g, uint8_t b, int thick) {
    // Clamp coordinates
    x1 = max(0, min(x1, width - 1));
    y1 = max(0, min(y1, height - 1));
    x2 = max(0, min(x2, width - 1));
    y2 = max(0, min(y2, height - 1));
    
    for (int t = 0; t < thick; t++) {
        // Top & bottom edges
        for (int x = x1; x <= x2; x++) {
            int yt = y1 + t;
            int yb = y2 - t;
            if (yt >= 0 && yt < height) {
                int idx = (yt * width + x) * 3;
                buf[idx] = r; buf[idx + 1] = g; buf[idx + 2] = b;
            }
            if (yb >= 0 && yb < height && yb != yt) {
                int idx = (yb * width + x) * 3;
                buf[idx] = r; buf[idx + 1] = g; buf[idx + 2] = b;
            }
        }
        // Left & right edges
        for (int y = y1; y <= y2; y++) {
            int xl = x1 + t;
            int xr = x2 - t;
            if (xl >= 0 && xl < width) {
                int idx = (y * width + xl) * 3;
                buf[idx] = r; buf[idx + 1] = g; buf[idx + 2] = b;
            }
            if (xr >= 0 && xr < width && xr != xl) {
                int idx = (y * width + xr) * 3;
                buf[idx] = r; buf[idx + 1] = g; buf[idx + 2] = b;
            }
        }
    }
}

// ==========================================
// STREAM HANDLER – MJPEG + Face Processing
// ==========================================
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    struct timeval _timestamp;
    esp_err_t res = ESP_OK;
    char buf[128];
    
    res = httpd_resp_set_type(req, STREAM_CT);
    if (res != ESP_OK) return res;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "10");
    
    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("[Stream] Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        
        _timestamp.tv_sec  = fb->timestamp.tv_sec;
        _timestamp.tv_usec = fb->timestamp.tv_usec;
        
        uint8_t *out_buf  = fb->buf;
        size_t   out_len  = fb->len;
        bool     need_free = false;
        
        // ---- FACE PROCESSING (esp-dl new API) ----
        // Chỉ xử lý mỗi 3 frame để giảm lag stream
        static int frame_skip = 0;
        bool do_detect = g_face_detect_on && s_detector && (++frame_skip % 3 == 0);
        if (do_detect) {
            size_t rgb_len = fb->width * fb->height * 3;
            uint8_t *rgb_buf = (uint8_t*)ps_malloc(rgb_len);

            if (rgb_buf && fmt2rgb888(fb->buf, fb->len, fb->format, rgb_buf)) {
                // --- DETECTION ---
                std::list<dl::detect::result_t> &results =
                    s_detector->infer(rgb_buf, {(int)fb->height, (int)fb->width, 3});

                if (!results.empty()) {
                    digitalWrite(LED_FLASH_PIN, HIGH);

                    int   recog_match_id = -1;
                    float recog_score    = 0;
                    bool  recog_enrolled = false;

                    // --- RECOGNITION (mặt đầu tiên) ---
                    // Dùng bounding box crop + resize 112x112, tránh landmark assertion crash
                    if (s_recognizer && (g_face_recognize_on || g_is_enrolling)) {
                        auto &first = results.front();

                        // Crop face từ bounding box rồi resize về 112×112
                        int bx1 = max(0, first.box[0]);
                        int by1 = max(0, first.box[1]);
                        int bx2 = min((int)fb->width  - 1, first.box[2]);
                        int by2 = min((int)fb->height - 1, first.box[3]);
                        int fw  = bx2 - bx1;
                        int fh  = by2 - by1;

                        if (fw > 10 && fh > 10) {
                            uint8_t *face_buf = (uint8_t*)ps_malloc(112 * 112 * 3);
                            if (face_buf) {
                                // Nearest-neighbor resize (fw×fh) → 112×112
                                for (int dy = 0; dy < 112; dy++) {
                                    for (int dx = 0; dx < 112; dx++) {
                                        int sx = bx1 + dx * fw / 112;
                                        int sy = by1 + dy * fh / 112;
                                        int si = (sy * fb->width + sx) * 3;
                                        int di = (dy * 112 + dx) * 3;
                                        face_buf[di]   = rgb_buf[si];
                                        face_buf[di+1] = rgb_buf[si+1];
                                        face_buf[di+2] = rgb_buf[si+2];
                                    }
                                }

                                dl::Tensor<uint8_t> face_tensor;
                                face_tensor.set_shape({112, 112, 3}).malloc_element();
                                memcpy(face_tensor.element, face_buf, 112 * 112 * 3);
                                free(face_buf);

                                if (g_is_enrolling && strlen(g_enroll_name) > 0) {
                                    // update_flash=false: lưu RAM (tránh lỗi "Please Set the Partition")
                                    int eid = s_recognizer->enroll_id(
                                        face_tensor, std::string(g_enroll_name), false);
                                    if (eid >= 0) {
                                        Serial.printf("[Face] ENROLLED: %s (ID %d)\n",
                                                      g_enroll_name, eid);
                                        recog_enrolled = true;
                                        strncpy(g_face_result_status, "ENROLLED", 15);
                                        strncpy(g_face_result_name, g_enroll_name, FACE_NAME_MAX - 1);
                                        g_face_result_score = 1.0f;
                                        g_face_result_pending = true;
                                        // Cập nhật cho web poll
                                        strncpy(g_last_face_status, "ENROLLED", 15);
                                        strncpy(g_last_face_name, g_enroll_name, FACE_NAME_MAX - 1);
                                        g_last_face_score = 1.0f;
                                        g_last_face_time  = millis();
                                    }
                                    g_is_enrolling = false;
                                    memset(g_enroll_name, 0, FACE_NAME_MAX);

                                } else if (g_face_recognize_on) {
                                    face_info_t info = s_recognizer->recognize(face_tensor);

                                    if (info.id >= 0) {
                                        recog_match_id = info.id;
                                        recog_score    = info.similarity;
                                        Serial.printf("[Face] MATCH: %s (ID %d, score=%.2f)\n",
                                                      info.name.c_str(), info.id, info.similarity);

                                        unsigned long now = millis();
                                        if (recog_match_id != g_last_match_id ||
                                            now - g_last_match_time > MATCH_COOLDOWN_MS) {
                                            strncpy(g_face_result_status, "MATCH", 15);
                                            strncpy(g_face_result_name, info.name.c_str(), FACE_NAME_MAX - 1);
                                            g_face_result_score = recog_score;
                                            g_face_result_pending = true;
                                            g_last_match_id   = recog_match_id;
                                            g_last_match_time = now;
                                            // Cập nhật cho web poll
                                            strncpy(g_last_face_status, "MATCH", 15);
                                            strncpy(g_last_face_name, info.name.c_str(), FACE_NAME_MAX - 1);
                                            g_last_face_score = recog_score;
                                            g_last_face_time  = now;
                                            mqtt_door_unlock();
                                        }
                                    } else {
                                        Serial.printf("[Face] NO_MATCH (score=%.2f)\n", info.similarity);
                                        unsigned long now = millis();
                                        if (now - g_last_nomatch_time > NOMATCH_COOLDOWN_MS) {
                                            strncpy(g_face_result_status, "NO_MATCH", 15);
                                            g_face_result_name[0] = '\0';
                                            g_face_result_score   = info.similarity;
                                            g_face_result_pending = true;
                                            g_last_nomatch_time   = now;
                                            // Cập nhật cho web poll
                                            strncpy(g_last_face_status, "NO_MATCH", 15);
                                            g_last_face_name[0] = '\0';
                                            g_last_face_score   = info.similarity;
                                            g_last_face_time    = now;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // --- DRAW BOUNDING BOXES ---
                    bool first = true;
                    for (auto &r : results) {
                        int bx1 = r.box[0], by1 = r.box[1];
                        int bx2 = r.box[2], by2 = r.box[3];
                        uint8_t cr = 255, cg = 255, cb = 0;  // vàng
                        if (first) {
                            if (recog_enrolled)        { cr=0;   cg=128; cb=255; } // xanh dương
                            else if (recog_match_id>=0){ cr=0;   cg=255; cb=0;   } // xanh lá
                            else if (g_face_recognize_on){ cr=255; cg=0;   cb=0; } // đỏ
                            first = false;
                        }
                        draw_rect_rgb888(rgb_buf, fb->width, fb->height,
                                         bx1, by1, bx2, by2, cr, cg, cb, 3);
                    }
                } else {
                    digitalWrite(LED_FLASH_PIN, LOW);
                }

                // Convert RGB888 → JPEG
                uint8_t *jpg_buf = NULL;
                size_t   jpg_len = 0;
                if (fmt2jpg(rgb_buf, rgb_len, fb->width, fb->height,
                            PIXFORMAT_RGB888, 80, &jpg_buf, &jpg_len) && jpg_buf) {
                    out_buf   = jpg_buf;
                    out_len   = jpg_len;
                    need_free = true;
                }
            }
            if (rgb_buf) free(rgb_buf);
        }
        
        // ---- SEND MJPEG FRAME ----
        size_t hlen = snprintf(buf, sizeof(buf), STREAM_PART, out_len,
                               _timestamp.tv_sec, _timestamp.tv_usec);
        
        res = httpd_resp_send_chunk(req, STREAM_BOUND, strlen(STREAM_BOUND));
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, buf, hlen);
        if (res == ESP_OK)
            res = httpd_resp_send_chunk(req, (const char*)out_buf, out_len);
        
        if (need_free && out_buf) free(out_buf);
        esp_camera_fb_return(fb);
        
        if (res != ESP_OK) break;
    }
    
    return res;
}

// ==========================================
// CAPTURE HANDLER – Chụp 1 ảnh JPEG  
// ==========================================
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// ==========================================
// STATUS HANDLER – Trạng thái hệ thống (JSON)
// ==========================================
static esp_err_t status_handler(httpd_req_t *req) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"wifi\":%s,\"rssi\":%d,\"mqtt\":%s,"
        "\"face_detect\":%s,\"face_recognize\":%s,"
        "\"face_count\":%d,\"free_heap\":%u,"
        "\"psram_free\":%u,\"uptime\":%lu}",
        WiFi.isConnected() ? "true" : "false",
        WiFi.RSSI(),
        mqtt_connected() ? "true" : "false",
        g_face_detect_on ? "true" : "false",
        g_face_recognize_on ? "true" : "false",
        s_recognizer ? s_recognizer->get_enrolled_id_num() : 0,
        ESP.getFreeHeap(),
        ESP.getFreePsram(),
        millis() / 1000
    );
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

// ==========================================
// CONTROL HANDLER – Điều khiển camera/face
// GET /control?var=xxx&val=yyy
// ==========================================
static esp_err_t control_handler(httpd_req_t *req) {
    char buf[128];
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len <= 1) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    if (buf_len > sizeof(buf)) buf_len = sizeof(buf);
    
    if (httpd_req_get_url_query_str(req, buf, buf_len) != ESP_OK) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    char var[32] = {0};
    char val[32] = {0};
    httpd_query_key_value(buf, "var", var, sizeof(var));
    httpd_query_key_value(buf, "val", val, sizeof(val));
    
    int value = atoi(val);
    
    // Process control commands
    if (strcmp(var, "face_detect") == 0) {
        g_face_detect_on = (value == 1);
        Serial.printf("[Control] Face detect: %s\n", g_face_detect_on ? "ON" : "OFF");
    }
    else if (strcmp(var, "face_recognize") == 0) {
        g_face_recognize_on = (value == 1);
        Serial.printf("[Control] Face recognize: %s\n", g_face_recognize_on ? "ON" : "OFF");
    }
    else if (strcmp(var, "flash") == 0) {
        digitalWrite(LED_FLASH_PIN, value ? HIGH : LOW);
        Serial.printf("[Control] Flash LED: %s\n", value ? "ON" : "OFF");
    }
    else if (strcmp(var, "framesize") == 0) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) s->set_framesize(s, (framesize_t)value);
    }
    else if (strcmp(var, "quality") == 0) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) s->set_quality(s, value);
    }
    else if (strcmp(var, "brightness") == 0) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) s->set_brightness(s, value);
    }
    else if (strcmp(var, "contrast") == 0) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) s->set_contrast(s, value);
    }
    else if (strcmp(var, "hmirror") == 0) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) s->set_hmirror(s, value);
    }
    else if (strcmp(var, "vflip") == 0) {
        sensor_t *s = esp_camera_sensor_get();
        if (s) s->set_vflip(s, value);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, "{\"ok\":true}", 11);
}

// ==========================================
// ENROLL HANDLER – Đăng ký khuôn mặt mới
// GET /enroll?name=Nghia
// ==========================================

// URL decode: %XX → ký tự, + → space
static void url_decode(char* dst, const char* src, int dst_size) {
    int i = 0, j = 0;
    while (src[i] && j < dst_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], 0};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

static esp_err_t enroll_handler(httpd_req_t *req) {
    char buf[128];
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    char raw_name[FACE_NAME_MAX] = {0};
    char name[FACE_NAME_MAX] = {0};

    if (buf_len > 1 && buf_len <= (int)sizeof(buf)) {
        httpd_req_get_url_query_str(req, buf, buf_len);
        httpd_query_key_value(buf, "name", raw_name, sizeof(raw_name));
        url_decode(name, raw_name, sizeof(name));
    }
    
    char json[192];
    if (strlen(name) == 0) {
        snprintf(json, sizeof(json), 
                 "{\"success\":false,\"message\":\"Missing 'name' parameter\"}");
    } else if (!g_face_detect_on) {
        snprintf(json, sizeof(json),
                 "{\"success\":false,\"message\":\"Enable Face Detection first!\"}");
    } else {
        // Bắt đầu enrollment – stream handler sẽ xử lý frame tiếp theo
        strncpy(g_enroll_name, name, FACE_NAME_MAX - 1);
        g_is_enrolling = true;
        
        snprintf(json, sizeof(json),
                 "{\"success\":true,\"message\":\"Enrolling '%s'. Look at camera...\"}",
                 name);
        Serial.printf("[Enroll] Started for: %s\n", name);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

// ==========================================
// DELETE FACE HANDLER – Xóa khuôn mặt
// GET /delete_face?id=0
// ==========================================
static esp_err_t delete_face_handler(httpd_req_t *req) {
    char buf[64];
    int buf_len = httpd_req_get_url_query_len(req) + 1;
    char id_str[8] = {0};
    
    if (buf_len > 1 && buf_len <= (int)sizeof(buf)) {
        httpd_req_get_url_query_str(req, buf, buf_len);
        httpd_query_key_value(buf, "id", id_str, sizeof(id_str));
    }
    
    int id = atoi(id_str);
    char json[128];

    bool deleted = s_recognizer && (s_recognizer->delete_id(id, true) >= 0);
    if (deleted) {
        snprintf(json, sizeof(json),
                 "{\"success\":true,\"message\":\"Deleted face #%d\"}", id);
    } else {
        snprintf(json, sizeof(json),
                 "{\"success\":false,\"message\":\"Face #%d not found\"}", id);
    }
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

// ==========================================
// LIST FACES HANDLER – Danh sách khuôn mặt (JSON)
// ==========================================
static esp_err_t list_faces_handler(httpd_req_t *req) {
    String json = "[";
    if (s_recognizer) {
        std::vector<face_info_t> ids = s_recognizer->get_enrolled_ids();
        for (size_t i = 0; i < ids.size(); i++) {
            if (i > 0) json += ",";
            json += "{\"id\":" + String(ids[i].id) +
                    ",\"name\":\"" + String(ids[i].name.c_str()) + "\"}";
        }
    }
    json += "]";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json.c_str(), json.length());
}

// ==========================================
// WEB UI – Embedded HTML/CSS/JS
// ==========================================
static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM Smart Home</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:'Segoe UI',system-ui,sans-serif;background:#0a0e17;color:#e0e6ed;min-height:100vh}
.hdr{background:linear-gradient(135deg,#0d1b3e 0%,#1a3a6c 50%,#0d4a7a 100%);padding:16px 20px;text-align:center;border-bottom:2px solid #1e88e5}
.hdr h1{font-size:22px;color:#fff;letter-spacing:1px}
.hdr p{font-size:12px;color:#90caf9;margin-top:4px}
.wrap{max-width:720px;margin:0 auto;padding:12px}
.card{background:linear-gradient(145deg,#111827,#1a2332);border:1px solid #1e3a5f;border-radius:12px;padding:16px;margin:12px 0;box-shadow:0 4px 15px rgba(0,0,0,0.3)}
.card h3{color:#64b5f6;margin-bottom:12px;font-size:15px}
.stream-box{background:#000;border-radius:12px;overflow:hidden;text-align:center;position:relative}
.stream-box img{width:100%;max-width:640px;display:block;margin:0 auto;min-height:200px;background:#111}
.stream-box .placeholder{padding:80px 20px;color:#455a64;font-size:14px}
.face-overlay{position:absolute;bottom:0;left:0;right:0;padding:10px 14px;background:linear-gradient(transparent,rgba(0,0,0,0.85));display:none;pointer-events:none}
.face-overlay .fo-name{font-size:1.1rem;font-weight:700;color:#fff;text-shadow:0 1px 4px rgba(0,0,0,0.8)}
.face-overlay .fo-score{font-size:0.75rem;color:rgba(255,255,255,0.7);margin-top:2px}
.face-overlay.match{border-bottom:3px solid #4caf50}
.face-overlay.nomatch{border-bottom:3px solid #f44336}
.face-overlay.enrolled{border-bottom:3px solid #2196f3}
.face-banner{display:none;border-radius:10px;padding:14px;margin:10px 0;text-align:center;animation:fadeIn .3s}
.face-banner.match{background:rgba(76,175,80,.15);border:1.5px solid #4caf50}
.face-banner.nomatch{background:rgba(244,67,54,.12);border:1.5px solid #f44336}
.face-banner.enrolled{background:rgba(33,150,243,.13);border:1.5px solid #2196f3}
.face-banner .fb-icon{font-size:2rem;line-height:1}
.face-banner .fb-name{font-size:1.4rem;font-weight:700;margin:4px 0 2px;color:#fff}
.face-banner .fb-sub{font-size:0.78rem;color:#90a4ae}
@keyframes fadeIn{from{opacity:0;transform:translateY(6px)}to{opacity:1;transform:none}}
.grid{display:grid;gap:8px}
.g2{grid-template-columns:1fr 1fr}
.g3{grid-template-columns:1fr 1fr 1fr}
.btn{padding:10px 14px;border:none;border-radius:8px;font-size:13px;cursor:pointer;transition:all .2s;font-weight:500;letter-spacing:0.5px}
.btn:active{transform:scale(0.96)}
.bp{background:linear-gradient(135deg,#1565c0,#1e88e5);color:#fff}
.bp:hover{background:linear-gradient(135deg,#1976d2,#42a5f5)}
.bs{background:linear-gradient(135deg,#2e7d32,#43a047);color:#fff}
.bs:hover{background:linear-gradient(135deg,#388e3c,#66bb6a)}
.bd{background:linear-gradient(135deg,#b71c1c,#d32f2f);color:#fff;padding:5px 12px;font-size:12px}
.bd:hover{background:linear-gradient(135deg,#c62828,#e53935)}
.ba{background:linear-gradient(135deg,#00897b,#26a69a);color:#fff}
.bon{background:linear-gradient(135deg,#e65100,#ff9800);color:#fff}
input[type=text],select{width:100%;padding:10px 12px;border:1px solid #37474f;border-radius:8px;background:#0d1117;color:#e0e6ed;font-size:14px;outline:none;transition:border .2s}
input[type=text]:focus,select:focus{border-color:#1e88e5}
.face-item{display:flex;justify-content:space-between;align-items:center;padding:10px 12px;background:#0d1117;border:1px solid #1e3a5f;border-radius:8px;margin:6px 0}
.face-item span{font-size:14px}
.face-item .fid{color:#64b5f6;font-weight:600;margin-right:8px}
.sbar{display:flex;justify-content:space-around;padding:10px;background:linear-gradient(145deg,#111827,#1a2332);border:1px solid #1e3a5f;border-radius:10px;margin:12px 0;font-size:11px}
.si{text-align:center}
.si .label{color:#78909c;margin-bottom:2px}
.si .val{font-weight:600;font-size:13px}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px;vertical-align:middle}
.dg{background:#4caf50}.dr{background:#f44336}.dy{background:#ff9800}
.toast{position:fixed;top:20px;right:20px;background:#1e88e5;color:#fff;padding:12px 20px;border-radius:8px;font-size:13px;z-index:999;display:none;box-shadow:0 4px 12px rgba(0,0,0,0.4)}
</style>
</head>
<body>
<div class="hdr">
<h1>🏠 Smart Home Camera</h1>
<p>ESP32-CAM AI Thinker • Face Recognition</p>
</div>
<div class="wrap">

<div class="card stream-box">
<div class="placeholder" id="ph">📷 Click "Start Stream" to begin</div>
<img id="stream" style="display:none" src="">
<div class="face-overlay" id="face-overlay">
<div class="fo-name" id="fo-name"></div>
<div class="fo-score" id="fo-score"></div>
</div>
</div>
<div class="face-banner" id="face-banner">
<div class="fb-icon" id="fb-icon"></div>
<div class="fb-name" id="fb-name"></div>
<div class="fb-sub"  id="fb-sub"></div>
</div>

<div class="card">
<div class="grid g2" style="margin-bottom:8px">
<button class="btn bp" id="btn-stream" onclick="toggleStream()">▶ Start Stream</button>
<button class="btn bp" onclick="capture()">📸 Capture</button>
</div>
<div class="grid g3">
<button class="btn bp" id="btn-det" onclick="toggleDetect()">👤 Detect OFF</button>
<button class="btn bp" id="btn-rec" onclick="toggleRecog()">🔍 Recog OFF</button>
<button class="btn bp" id="btn-flash" onclick="toggleFlash()">💡 Flash</button>
</div>
</div>

<div class="card">
<h3>📸 Enroll New Face</h3>
<input type="text" id="ename" placeholder="Enter name..." maxlength="31">
<button class="btn bs" onclick="enrollFace()" style="width:100%;margin-top:8px">✅ Enroll Face</button>
<p style="font-size:11px;color:#78909c;margin-top:6px">
⚠️ Enable Detection + start stream. Look at camera when enrolling.
</p>
</div>

<div class="card">
<h3>👥 Enrolled Faces (<span id="fcount">0</span>)</h3>
<div id="flist"><p style="color:#455a64">Loading...</p></div>
</div>

<div class="card">
<h3>⚙️ Camera Settings</h3>
<div class="grid g2">
<div>
<label style="font-size:12px;color:#90a4ae">Resolution</label>
<select onchange="setCtrl('framesize',this.value)">
<option value="10" selected>QVGA (320×240)</option>
<option value="8">VGA (640×480)</option>
<option value="5">CIF (400×296)</option>
<option value="3">HQVGA (240×176)</option>
</select>
</div>
<div>
<label style="font-size:12px;color:#90a4ae">Quality (1-63)</label>
<select onchange="setCtrl('quality',this.value)">
<option value="10" selected>10 (Best)</option>
<option value="20">20 (Good)</option>
<option value="30">30 (Medium)</option>
<option value="50">50 (Low)</option>
</select>
</div>
</div>
<div class="grid g3" style="margin-top:8px">
<button class="btn bp" onclick="setCtrl('hmirror',1)">🔄 Mirror</button>
<button class="btn bp" onclick="setCtrl('vflip',1)">🔃 Flip</button>
<button class="btn bp" onclick="setCtrl('brightness',1)">🔆 Bright+</button>
</div>
</div>

<div class="sbar" id="sbar">
<div class="si"><div class="label">WiFi</div><div class="val"><span class="dot dg" id="wifi-dot"></span><span id="rssi">--</span>dBm</div></div>
<div class="si"><div class="label">MQTT</div><div class="val"><span class="dot dr" id="mqtt-dot"></span><span id="mqtt-st">--</span></div></div>
<div class="si"><div class="label">Heap</div><div class="val" id="heap">--</div></div>
<div class="si"><div class="label">PSRAM</div><div class="val" id="psram">--</div></div>
<div class="si"><div class="label">Uptime</div><div class="val" id="uptime">--</div></div>
</div>

</div>
<div class="toast" id="toast"></div>

<script>
const B=window.location.origin;
const S='http://'+window.location.hostname+':81/stream';
let sOn=false,dOn=false,rOn=false,fOn=false;

function toast(m){const t=document.getElementById('toast');t.textContent=m;t.style.display='block';setTimeout(()=>t.style.display='none',3000)}

function toggleStream(){
sOn=!sOn;
const img=document.getElementById('stream'),ph=document.getElementById('ph'),btn=document.getElementById('btn-stream');
if(sOn){img.src=S;img.style.display='block';ph.style.display='none';btn.textContent='⏹ Stop Stream';btn.className='btn bon'}
else{img.src='';img.style.display='none';ph.style.display='block';btn.textContent='▶ Start Stream';btn.className='btn bp'}
}

function toggleDetect(){
dOn=!dOn;
fetch(B+'/control?var=face_detect&val='+(dOn?1:0));
const b=document.getElementById('btn-det');
b.textContent='👤 Detect '+(dOn?'ON':'OFF');
b.className='btn '+(dOn?'ba':'bp');
toast('Face Detection '+(dOn?'ON':'OFF'));
}

function toggleRecog(){
rOn=!rOn;
fetch(B+'/control?var=face_recognize&val='+(rOn?1:0));
const b=document.getElementById('btn-rec');
b.textContent='🔍 Recog '+(rOn?'ON':'OFF');
b.className='btn '+(rOn?'ba':'bp');
toast('Face Recognition '+(rOn?'ON':'OFF'));
}

function toggleFlash(){
fOn=!fOn;
fetch(B+'/control?var=flash&val='+(fOn?1:0));
const b=document.getElementById('btn-flash');
b.className='btn '+(fOn?'bon':'bp');
}

function capture(){window.open(B+'/capture','_blank')}

function setCtrl(v,val){fetch(B+'/control?var='+v+'&val='+val)}

function enrollFace(){
const n=document.getElementById('ename').value.trim();
if(!n){toast('⚠️ Enter a name first!');return}
fetch(B+'/enroll?name='+encodeURIComponent(n))
.then(r=>r.json()).then(d=>{toast(d.message);if(d.success)loadFaces()})
.catch(()=>toast('❌ Failed to enroll'));
document.getElementById('ename').value='';
}

function deleteFace(id){
if(!confirm('Delete face #'+id+'?'))return;
fetch(B+'/delete_face?id='+id)
.then(r=>r.json()).then(d=>{toast(d.message);loadFaces()})
.catch(()=>toast('❌ Failed to delete'));
}

function loadFaces(){
fetch(B+'/list_faces').then(r=>r.json()).then(f=>{
const el=document.getElementById('flist');
document.getElementById('fcount').textContent=f.length;
if(f.length===0){el.innerHTML='<p style="color:#455a64;text-align:center">No faces enrolled</p>';return}
el.innerHTML=f.map(x=>'<div class="face-item"><span><span class="fid">#'+x.id+'</span>'+x.name+'</span><button class="btn bd" onclick="deleteFace('+x.id+')">✕ Delete</button></div>').join('');
}).catch(()=>{});
}

function updateStatus(){
fetch(B+'/status').then(r=>r.json()).then(s=>{
document.getElementById('wifi-dot').className='dot '+(s.wifi?'dg':'dr');
document.getElementById('rssi').textContent=s.rssi;
document.getElementById('mqtt-dot').className='dot '+(s.mqtt?'dg':'dr');
document.getElementById('mqtt-st').textContent=s.mqtt?'OK':'OFF';
document.getElementById('heap').textContent=Math.round(s.free_heap/1024)+'KB';
document.getElementById('psram').textContent=Math.round(s.psram_free/1024)+'KB';
const m=Math.floor(s.uptime/60),h=Math.floor(m/60);
document.getElementById('uptime').textContent=(h>0?h+'h ':'')+(m%60)+'m';
}).catch(()=>{});
}

let _faceTimer=null,_lastFaceTime=0;
function showFace(s,name,score){
const cfg={
MATCH:{cls:'match',icon:'✅',n:name||'—',sub:'Đã nhận diện • Score: '+(score*100).toFixed(0)+'%'},
NO_MATCH:{cls:'nomatch',icon:'❌',n:'Không nhận ra',sub:'Score: '+(score*100).toFixed(0)+'%'},
ENROLLED:{cls:'enrolled',icon:'➕',n:name||'—',sub:'Đã đăng ký thành công'},
}[s];
if(!cfg)return;
const ov=document.getElementById('face-overlay');
ov.className='face-overlay '+cfg.cls;
document.getElementById('fo-name').textContent=s==='MATCH'?'👤 '+cfg.n:s==='ENROLLED'?'➕ '+cfg.n:'❓ Không nhận ra';
document.getElementById('fo-score').textContent='Điểm tương đồng: '+(score*100).toFixed(0)+'%';
ov.style.display='block';
const bn=document.getElementById('face-banner');
bn.className='face-banner '+cfg.cls;
document.getElementById('fb-icon').textContent=cfg.icon;
document.getElementById('fb-name').textContent=cfg.n;
document.getElementById('fb-sub').textContent=cfg.sub;
bn.style.display='block';bn.style.animation='none';bn.offsetHeight;bn.style.animation='';
clearTimeout(_faceTimer);
_faceTimer=setTimeout(()=>{ov.style.display='none';bn.style.display='none';},5000);
}
function pollFace(){
fetch(B+'/face_result').then(r=>r.json()).then(d=>{
if(d.time&&d.time!==_lastFaceTime){_lastFaceTime=d.time;showFace(d.status,d.name,d.score||0);}
}).catch(()=>{});
}
loadFaces();
updateStatus();
setInterval(updateStatus,5000);
setInterval(loadFaces,15000);
setInterval(pollFace,1000);
</script>
</body>
</html>
)rawliteral";

// ==========================================
// FACE RESULT HANDLER – Kết quả nhận diện mới nhất (JSON)
// GET /face_result → {"status":"MATCH","name":"Nghia","score":0.89,"time":12345}
// ==========================================
static esp_err_t face_result_handler(httpd_req_t *req) {
    char json[192];
    snprintf(json, sizeof(json),
        "{\"status\":\"%s\",\"name\":\"%s\",\"score\":%.3f,\"time\":%lu}",
        g_last_face_status,
        g_last_face_name,
        g_last_face_score,
        g_last_face_time
    );
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json, strlen(json));
}

// ==========================================
// INDEX HANDLER – Serve Web UI
// ==========================================
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

// ==========================================
// URI DEFINITIONS
// ==========================================
static const httpd_uri_t uri_index = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_stream = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = stream_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_capture = {
    .uri      = "/capture",
    .method   = HTTP_GET,
    .handler  = capture_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_status = {
    .uri      = "/status",
    .method   = HTTP_GET,
    .handler  = status_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_control = {
    .uri      = "/control",
    .method   = HTTP_GET,
    .handler  = control_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_enroll = {
    .uri      = "/enroll",
    .method   = HTTP_GET,
    .handler  = enroll_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_delete = {
    .uri      = "/delete_face",
    .method   = HTTP_GET,
    .handler  = delete_face_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_list = {
    .uri      = "/list_faces",
    .method   = HTTP_GET,
    .handler  = list_faces_handler,
    .user_ctx = NULL
};
static const httpd_uri_t uri_face_result = {
    .uri      = "/face_result",
    .method   = HTTP_GET,
    .handler  = face_result_handler,
    .user_ctx = NULL
};

// ==========================================
// START CAMERA SERVER
// Gọi từ Web_server.ino setup()
// ==========================================
void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    
    // Khởi tạo face detector và recognizer (esp-dl API)
    s_detector   = new HumanFaceDetectMSR01(0.3F, 0.3F, 10, 0.3F);
    s_recognizer = new FaceRecognition112V1S8();
    s_recognizer->set_thresh(FACE_MATCH_THRESHOLD);
    Serial.println("[Camera] Face detection: ENABLED");
    Serial.println("[Camera] Face recognition: ENABLED");
    
    // Camera Server – Port 80 (Web UI + API)
    Serial.println("[Camera] Starting web server on port 80...");
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &uri_index);
        httpd_register_uri_handler(camera_httpd, &uri_capture);
        httpd_register_uri_handler(camera_httpd, &uri_status);
        httpd_register_uri_handler(camera_httpd, &uri_control);
        httpd_register_uri_handler(camera_httpd, &uri_enroll);
        httpd_register_uri_handler(camera_httpd, &uri_delete);
        httpd_register_uri_handler(camera_httpd, &uri_list);
        httpd_register_uri_handler(camera_httpd, &uri_face_result);
        Serial.println("[Camera] Web server OK (port 80)");
    }
    
    // Stream Server – Port 81 (MJPEG stream riêng để không block web UI)
    config.server_port += 1;      // Port 81
    config.ctrl_port   += 1;      // Control port 32769
    Serial.println("[Camera] Starting stream server on port 81...");
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &uri_stream);
        Serial.println("[Camera] Stream server OK (port 81)");
    }
}
