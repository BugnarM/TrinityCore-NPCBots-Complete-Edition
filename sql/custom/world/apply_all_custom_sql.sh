#!/usr/bin/env bash
# Script to apply all custom SQL migrations for TrinityCore-NPCBots-Complete-Edition
set -euo pipefail

DB_USER="${1:-trinity}"
DB_PASS="${2:-trinity}"
DB_NAME="${3:-world}"
DB_HOST="${4:-127.0.0.1}"
DB_PORT="${5:-3306}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "============================================================="
echo "Applying Custom SQL Updates to ${DB_NAME} on ${DB_HOST}:${DB_PORT}"
echo "============================================================="

for sql_file in $(ls -1 "${SCRIPT_DIR}"/*.sql | sort); do
    filename="$(basename "${sql_file}")"
    echo "Applying: ${filename}..."
    mysql -h "${DB_HOST}" -P "${DB_PORT}" -u "${DB_USER}" -p"${DB_PASS}" "${DB_NAME}" < "${sql_file}"
done

echo "============================================================="
echo "All custom SQL updates applied successfully!"
echo "============================================================="
