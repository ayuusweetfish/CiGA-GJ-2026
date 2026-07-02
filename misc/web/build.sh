LOVEJS_ROOT=/mnt/data/artf/lovejs/love.js

TITLE="CiGA Game Jam 2026, 🐟🐐🍎🌰’s Game"
LOVE_FILE=/tmp/CGJ2026_🐟🐐🍎🌰/CGJ2026_🐟🐐🍎🌰.love
OUTPUT_ROOT=/tmp/CGJ2026_🐟🐐🍎🌰/CGJ2026_🐟🐐🍎🌰_web
MEMORY=$((64 * 1048576))

rm -rf "${OUTPUT_ROOT}"
deno run --allow-env --allow-read="${LOVEJS_ROOT}" --allow-read="${LOVE_FILE}" --allow-read="${OUTPUT_ROOT}" --allow-write="${OUTPUT_ROOT}" --unstable-detect-cjs "${LOVEJS_ROOT}/index.js" --title "${TITLE}" -c -m ${MEMORY} "${LOVE_FILE}" "${OUTPUT_ROOT}"
rm -rf "${OUTPUT_ROOT}/theme"
cp -p misc/web/index.html "${OUTPUT_ROOT}/index.html"
