LOVE_FILE="/tmp/CGJ2026_🐟🐐🍎🌰/CGJ2026_🐟🐐🍎🌰.love"

t=$(mktemp -d)
wd=$PWD
cp -pr aud img main.lua src "$t/"
(
  cd "$t/" || exit
  if [ "$1" != "nomin" ]; then
    echo Minifying
    find . -type f -name '*.lua' | while read i; do
      deno run "$wd/misc/luamin-env/node_modules/luamin/bin/luamin" -c < "$i" > "$t/_tmp"
      mv "$t/_tmp" "$i"
    done
  fi
  find . -exec touch -t 198001010000 {} +
  rm -f ${LOVE_FILE}
  mkdir -p "$(dirname "${LOVE_FILE}")"
  find . -type f -print | sort | zip ${LOVE_FILE} -X -@ -9
  sha1sum ${LOVE_FILE}
)

if [ -n "$USE_BOON" ]; then
  BOON=${BOON:-/mnt/data/tools/boon/target/release/boon}
  cp misc/Boon.toml "$t/"
  (cd "$t"; ${BOON} build . --target all)
fi

rm -rf "$t"
