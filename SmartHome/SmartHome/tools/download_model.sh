#!/bin/bash
# ============================================================
#  download_model.sh – Tải TF Micro Speech pre-trained model
#  Smart Home VKU v2.0 – Phase 4
# ============================================================
#  Script này tải model TF Micro Speech (Yes/No) từ
#  TensorFlow Lite Micro repository và convert sang C header.
#
#  Cách dùng:
#    chmod +x tools/download_model.sh
#    ./tools/download_model.sh
#
#  Output: Esp32_Voice/model_data.h
# ============================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
OUTPUT_FILE="$PROJECT_ROOT/Esp32_Voice/model_data.h"

echo "╔══════════════════════════════════════════╗"
echo "║  TF Micro Speech Model Downloader        ║"
echo "║  Smart Home VKU v2.0                     ║"
echo "╚══════════════════════════════════════════╝"
echo ""

# ==================== Config ====================
# URL model micro_speech từ Google Storage (zip bundle chính thức)
MODEL_ZIP_URL="https://storage.googleapis.com/download.tensorflow.org/models/tflite/micro/micro_speech_nov_2021.zip"
TEMP_ZIP="/tmp/micro_speech.zip"
TEMP_DIR="/tmp/micro_speech_model"
TEMP_FILE="$TEMP_DIR/micro_speech_model_data.cc"

# ==================== Download ====================
echo "[1/4] Đang tải model bundle từ Google Storage..."
echo "      URL: $MODEL_ZIP_URL"
echo ""

mkdir -p "$TEMP_DIR"

if command -v curl &> /dev/null; then
    curl -L --progress-bar "$MODEL_ZIP_URL" -o "$TEMP_ZIP"
elif command -v wget &> /dev/null; then
    wget --show-progress -q "$MODEL_ZIP_URL" -O "$TEMP_ZIP"
else
    echo "❌ LỖI: Cần curl hoặc wget. Hãy cài đặt một trong hai."
    exit 1
fi

if [ ! -f "$TEMP_ZIP" ] || [ ! -s "$TEMP_ZIP" ]; then
    echo "❌ LỖI: Tải file thất bại hoặc file rỗng."
    exit 1
fi

echo ""
echo "[2/4] Đang giải nén..."
unzip -o "$TEMP_ZIP" -d "$TEMP_DIR" > /dev/null 2>&1

# Tìm file model_data.cc trong thư mục giải nén
FOUND_CC=$(find "$TEMP_DIR" -name "*model_data*.cc" 2>/dev/null | head -1)
if [ -z "$FOUND_CC" ]; then
    FOUND_CC=$(find "$TEMP_DIR" -name "*.cc" 2>/dev/null | head -1)
fi

if [ -z "$FOUND_CC" ]; then
    echo "❌ LỖI: Không tìm thấy file .cc trong zip."
    echo "   Nội dung zip:"
    unzip -l "$TEMP_ZIP" | head -20
    exit 1
fi

TEMP_FILE="$FOUND_CC"
echo "   Tìm thấy: $TEMP_FILE"

echo ""
echo "[3/4] Đang convert .cc → .h header file..."

# ==================== Convert ====================
# Tạo header file từ .cc với:
# 1. Thêm #ifndef guards
# 2. Giữ nguyên array data
# 3. Thêm extern declaration

cat > "$OUTPUT_FILE" << 'HEADER_START'
/*
 * ============================================================
 *  model_data.h – TF Micro Speech Pre-trained Model
 *  Smart Home VKU v2.0 – Phase 4
 * ============================================================
 *  Pre-trained model nhận diện: "yes", "no", "silence", "unknown"
 *  Source: TensorFlow Lite Micro Examples (micro_speech)
 *  Classes: [silence=0, unknown=1, yes=2, no=3]
 *
 *  Input:  1960 int8 values (49 MFCC frames × 40 coefficients)
 *  Output: 4 int8 scores (dequantize để lấy float 0-1)
 *
 *  AUTO-GENERATED bởi tools/download_model.sh
 *  KHÔNG chỉnh sửa tay – chạy lại script để update model
 * ============================================================
 */

#ifndef MODEL_DATA_H
#define MODEL_DATA_H

HEADER_START

# Extract chỉ phần array data từ .cc file
# Tìm dòng "const unsigned char" hoặc "alignas"
if grep -q "alignas" "$TEMP_FILE"; then
    # Format mới có alignas
    grep -A 999999 "alignas" "$TEMP_FILE" | \
    sed 's/const unsigned char g_model\[\]/extern const unsigned char g_model[]/' \
    >> "$OUTPUT_FILE"
else
    # Format cũ không có alignas
    grep -A 999999 "const unsigned char g_model" "$TEMP_FILE" \
    >> "$OUTPUT_FILE"
fi

# Thêm const int g_model_len nếu có trong .cc
if grep -q "g_model_len" "$TEMP_FILE"; then
    echo "" >> "$OUTPUT_FILE"
    grep "g_model_len" "$TEMP_FILE" >> "$OUTPUT_FILE"
fi

echo "" >> "$OUTPUT_FILE"
echo "#endif // MODEL_DATA_H" >> "$OUTPUT_FILE"

# ==================== Verify ====================
echo "[4/4] Xác minh output..."

if [ -f "$OUTPUT_FILE" ]; then
    SIZE=$(wc -c < "$OUTPUT_FILE")
    LINES=$(wc -l < "$OUTPUT_FILE")
    echo ""
    echo "✅ Tạo thành công: $OUTPUT_FILE"
    echo "   Kích thước: ${SIZE} bytes | ${LINES} dòng"
    echo ""

    # Kiểm tra có chứa array data không
    if grep -q "g_model\[\]" "$OUTPUT_FILE"; then
        echo "✅ Model array hợp lệ (g_model[] tìm thấy)"
    else
        echo "⚠️  CẢNH BÁO: Không tìm thấy g_model[] – kiểm tra file output"
    fi

    if grep -q "g_model_len" "$OUTPUT_FILE"; then
        MODEL_SIZE=$(grep "g_model_len" "$OUTPUT_FILE" | grep -oE "[0-9]+" | head -1)
        echo "✅ Model size: ${MODEL_SIZE} bytes"
    fi
else
    echo "❌ LỖI: Không tạo được output file!"
    exit 1
fi

# Cleanup
rm -f "$TEMP_ZIP"
rm -rf "$TEMP_DIR"

echo ""
echo "══════════════════════════════════════════"
echo "📋 BƯỚC TIẾP THEO:"
echo ""
echo "1. Mở Arduino IDE"
echo "2. Cài thư viện: TensorFlowLite_ESP32 (Library Manager)"
echo "3. Mở Esp32_Voice/Voice.ino"
echo "4. Trong config.h, bỏ comment dòng: #define USE_TFLITE_MODEL"
echo "5. Upload lên ESP32 (ESP32 Board Core 2.0.x)"
echo "6. Mở Serial Monitor (115200 baud)"
echo "7. Nói 'Yes' → đèn bật | Nói 'No' → đèn tắt"
echo "══════════════════════════════════════════"
echo ""
