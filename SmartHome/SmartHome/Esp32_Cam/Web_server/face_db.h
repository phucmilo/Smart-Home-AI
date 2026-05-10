#pragma once
// ============================================
// face_db.h – Face Database Interface (SPIFFS)
// Lưu trữ face enrollment data persistent
// ============================================

#include <Arduino.h>
#include "config.h"

// ===== Constants =====
// FACE_NAME_MAX đã được define trong config.h (= 32)
#define FACE_EMBED_SIZE     512     // Kích thước embedding vector (int8_t)
#define FACE_MAX_DB         50      // Tối đa 50 khuôn mặt

// ===== Face Entry Structure =====
typedef struct {
    bool    valid;                          // Slot đang được sử dụng?
    char    name[FACE_NAME_MAX];           // Tên người
    int8_t  embedding[FACE_EMBED_SIZE];    // Face embedding vector
} face_entry_t;

// ===== Public API =====

// Khởi tạo SPIFFS và load face database từ flash
void face_db_init();

// Enroll face mới vào database
// Trả về: ID (>= 0) nếu thành công, -1 nếu database đầy
int face_db_enroll(const char* name, const int8_t* embedding);

// Tìm face khớp nhất bằng cosine similarity
// Trả về: ID (>= 0) nếu tìm thấy match > threshold, -1 nếu không khớp
// out_score: điểm similarity cao nhất (output)
int face_db_match(const int8_t* embedding, float threshold, float* out_score);

// Lấy tên theo ID
const char* face_db_get_name(int id);

// Xóa face theo ID
bool face_db_delete(int id);

// Số lượng faces đã enroll
int face_db_count();

// Danh sách faces dưới dạng JSON (cho Web API)
// Format: [{"id":0,"name":"Nghia"},{"id":3,"name":"An"}]
String face_db_list_json();
