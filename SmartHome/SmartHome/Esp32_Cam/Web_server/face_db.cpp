// ============================================
// face_db.cpp – Face Database Implementation
// SPIFFS-based persistent face enrollment storage
// Smart Home VKU v2.0 – Phase 3
// ============================================

#include "face_db.h"
#include <SPIFFS.h>
#include <math.h>

// ===== Private: Face database array (allocated in PSRAM if available) =====
static face_entry_t* face_db = NULL;
static bool db_initialized = false;

// ===== Helper: Cosine Similarity (int8_t arrays) =====
// Tính cosine similarity giữa 2 face embedding vectors
// Trả về giá trị từ -1.0 đến 1.0 (1.0 = giống nhau hoàn toàn)
static float cosine_similarity(const int8_t* a, const int8_t* b, int size) {
    int32_t dot   = 0;
    int32_t norm_a = 0;
    int32_t norm_b = 0;
    
    for (int i = 0; i < size; i++) {
        dot    += (int32_t)a[i] * (int32_t)b[i];
        norm_a += (int32_t)a[i] * (int32_t)a[i];
        norm_b += (int32_t)b[i] * (int32_t)b[i];
    }
    
    if (norm_a == 0 || norm_b == 0) return 0.0f;
    return (float)dot / (sqrtf((float)norm_a) * sqrtf((float)norm_b));
}

// ===== Helper: Get SPIFFS file path for face ID =====
static String face_file_path(int id) {
    return "/f" + String(id) + ".bin";
}

// ===== Helper: Save single face to SPIFFS =====
static bool save_face_to_spiffs(int id) {
    if (!face_db || id < 0 || id >= FACE_MAX_DB || !face_db[id].valid) {
        return false;
    }
    
    String path = face_file_path(id);
    File f = SPIFFS.open(path, "w");
    if (!f) {
        Serial.printf("[FaceDB] ERROR: Cannot open %s for write\n", path.c_str());
        return false;
    }
    
    // Write: name (FACE_NAME_MAX bytes) + embedding (FACE_EMBED_SIZE bytes)
    size_t written = 0;
    written += f.write((uint8_t*)face_db[id].name, FACE_NAME_MAX);
    written += f.write((uint8_t*)face_db[id].embedding, FACE_EMBED_SIZE);
    f.close();
    
    size_t expected = FACE_NAME_MAX + FACE_EMBED_SIZE;
    if (written != expected) {
        Serial.printf("[FaceDB] ERROR: Write %d/%d bytes\n", written, expected);
        return false;
    }
    
    Serial.printf("[FaceDB] Saved face #%d (%s) → %s (%d bytes)\n", 
                  id, face_db[id].name, path.c_str(), written);
    return true;
}

// ===== Helper: Load single face from SPIFFS =====
static bool load_face_from_spiffs(int id) {
    if (!face_db || id < 0 || id >= FACE_MAX_DB) return false;
    
    String path = face_file_path(id);
    if (!SPIFFS.exists(path)) return false;
    
    File f = SPIFFS.open(path, "r");
    if (!f) return false;
    
    size_t expected = FACE_NAME_MAX + FACE_EMBED_SIZE;
    if (f.size() != expected) {
        Serial.printf("[FaceDB] WARNING: %s size mismatch (%d != %d)\n", 
                      path.c_str(), f.size(), expected);
        f.close();
        return false;
    }
    
    f.read((uint8_t*)face_db[id].name, FACE_NAME_MAX);
    f.read((uint8_t*)face_db[id].embedding, FACE_EMBED_SIZE);
    face_db[id].name[FACE_NAME_MAX - 1] = '\0';  // Đảm bảo null-terminated
    face_db[id].valid = true;
    f.close();
    
    return true;
}

// ============================================
// PUBLIC API
// ============================================

void face_db_init() {
    // Allocate face database array
    // Ưu tiên PSRAM (27KB) vì ESP32-CAM có 4MB PSRAM
    if (psramFound()) {
        face_db = (face_entry_t*)ps_calloc(FACE_MAX_DB, sizeof(face_entry_t));
        Serial.printf("[FaceDB] Allocated %d bytes in PSRAM\n", 
                      FACE_MAX_DB * sizeof(face_entry_t));
    } else {
        face_db = (face_entry_t*)calloc(FACE_MAX_DB, sizeof(face_entry_t));
        Serial.println("[FaceDB] WARNING: PSRAM not found! Using internal RAM");
    }
    
    if (!face_db) {
        Serial.println("[FaceDB] FATAL: Cannot allocate face database!");
        return;
    }
    
    // Clear all entries
    for (int i = 0; i < FACE_MAX_DB; i++) {
        face_db[i].valid = false;
    }
    
    // Initialize SPIFFS
    if (!SPIFFS.begin(true)) {
        Serial.println("[FaceDB] ERROR: SPIFFS mount failed!");
        return;
    }
    Serial.printf("[FaceDB] SPIFFS: Total=%dKB, Used=%dKB\n", 
                  SPIFFS.totalBytes() / 1024, SPIFFS.usedBytes() / 1024);
    
    // Load all faces from SPIFFS
    int loaded = 0;
    for (int i = 0; i < FACE_MAX_DB; i++) {
        if (load_face_from_spiffs(i)) {
            loaded++;
            Serial.printf("[FaceDB] Loaded: #%d → %s\n", i, face_db[i].name);
        }
    }
    Serial.printf("[FaceDB] Initialized: %d faces loaded from SPIFFS\n", loaded);
    
    db_initialized = true;
}

int face_db_enroll(const char* name, const int8_t* embedding) {
    if (!face_db || !name || !embedding) return -1;
    
    // Tìm slot trống
    int id = -1;
    for (int i = 0; i < FACE_MAX_DB; i++) {
        if (!face_db[i].valid) {
            id = i;
            break;
        }
    }
    
    if (id < 0) {
        Serial.println("[FaceDB] ERROR: Database full!");
        return -1;
    }
    
    // Lưu vào memory
    face_db[id].valid = true;
    strncpy(face_db[id].name, name, FACE_NAME_MAX - 1);
    face_db[id].name[FACE_NAME_MAX - 1] = '\0';
    memcpy(face_db[id].embedding, embedding, FACE_EMBED_SIZE);
    
    // Persist to SPIFFS
    if (!save_face_to_spiffs(id)) {
        Serial.println("[FaceDB] WARNING: Failed to save to SPIFFS (RAM OK)");
    }
    
    Serial.printf("[FaceDB] Enrolled: #%d → '%s'\n", id, name);
    return id;
}

int face_db_match(const int8_t* embedding, float threshold, float* out_score) {
    if (!face_db || !embedding) return -1;
    
    float best_score = -1.0f;
    int   best_id    = -1;
    
    for (int i = 0; i < FACE_MAX_DB; i++) {
        if (!face_db[i].valid) continue;
        
        float score = cosine_similarity(embedding, face_db[i].embedding, FACE_EMBED_SIZE);
        if (score > best_score) {
            best_score = score;
            best_id    = i;
        }
    }
    
    if (out_score) *out_score = best_score;
    
    // Chỉ trả về match nếu vượt ngưỡng
    if (best_id >= 0 && best_score >= threshold) {
        return best_id;
    }
    
    return -1;  // Không khớp
}

const char* face_db_get_name(int id) {
    if (!face_db || id < 0 || id >= FACE_MAX_DB || !face_db[id].valid) {
        return "Unknown";
    }
    return face_db[id].name;
}

bool face_db_delete(int id) {
    if (!face_db || id < 0 || id >= FACE_MAX_DB || !face_db[id].valid) {
        return false;
    }
    
    // Xóa khỏi memory
    face_db[id].valid = false;
    memset(face_db[id].name, 0, FACE_NAME_MAX);
    memset(face_db[id].embedding, 0, FACE_EMBED_SIZE);
    
    // Xóa file SPIFFS
    String path = face_file_path(id);
    if (SPIFFS.exists(path)) {
        SPIFFS.remove(path);
    }
    
    Serial.printf("[FaceDB] Deleted face #%d\n", id);
    return true;
}

int face_db_count() {
    if (!face_db) return 0;
    int count = 0;
    for (int i = 0; i < FACE_MAX_DB; i++) {
        if (face_db[i].valid) count++;
    }
    return count;
}

String face_db_list_json() {
    String json = "[";
    bool first = true;
    
    for (int i = 0; i < FACE_MAX_DB; i++) {
        if (!face_db || !face_db[i].valid) continue;
        
        if (!first) json += ",";
        first = false;
        
        json += "{\"id\":";
        json += String(i);
        json += ",\"name\":\"";
        json += String(face_db[i].name);
        json += "\"}";
    }
    
    json += "]";
    return json;
}
