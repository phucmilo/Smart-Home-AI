#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CERT_DIR="$ROOT_DIR/certs"
KEY_PATH="$CERT_DIR/lan-key.pem"
CERT_PATH="$CERT_DIR/lan-cert.pem"

mkdir -p "$CERT_DIR"

IPS=("127.0.0.1")

add_ip() {
  local candidate="${1:-}"

  [[ -z "$candidate" ]] && return
  [[ "$candidate" == 127.* ]] && return
  [[ "$candidate" == 169.254.* ]] && return

  for existing in "${IPS[@]}"; do
    [[ "$existing" == "$candidate" ]] && return
  done

  IPS+=("$candidate")
}

if [[ "$#" -gt 0 ]]; then
  for arg in "$@"; do
    add_ip "$arg"
  done
else
  while IFS= read -r ip; do
    add_ip "$ip"
  done < <(ifconfig 2>/dev/null | awk '/inet / {print $2}')
fi

TMP_CONF="$(mktemp)"
trap 'rm -f "$TMP_CONF"' EXIT

{
  echo '[req]'
  echo 'default_bits = 2048'
  echo 'prompt = no'
  echo 'default_md = sha256'
  echo 'x509_extensions = req_ext'
  echo 'distinguished_name = dn'
  echo '[dn]'
  echo 'CN = SmartHome LAN'
  echo '[req_ext]'
  echo 'subjectAltName = @alt_names'
  echo '[alt_names]'
  echo 'DNS.1 = localhost'
  echo 'IP.1 = 127.0.0.1'

  idx=2
  for ip in "${IPS[@]}"; do
    [[ "$ip" == '127.0.0.1' ]] && continue
    echo "IP.${idx} = ${ip}"
    idx=$((idx + 1))
  done
} > "$TMP_CONF"

openssl req \
  -x509 \
  -nodes \
  -newkey rsa:2048 \
  -days 825 \
  -keyout "$KEY_PATH" \
  -out "$CERT_PATH" \
  -config "$TMP_CONF" \
  -extensions req_ext

echo "Generated:"
echo "  $KEY_PATH"
echo "  $CERT_PATH"
echo
echo "IPs in certificate:"
for ip in "${IPS[@]}"; do
  echo "  - $ip"
done
