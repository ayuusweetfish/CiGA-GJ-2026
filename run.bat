:<<BATCH
  start .\misc\love-11.5-win64\love.exe .
  exit /b
BATCH

wd=$(dirname "$0")

if [ "$(uname -s | tr '[:upper:]' '[:lower:]')" = "darwin" ]; then
  "$wd/misc/love.app/Contents/MacOS/love" "$wd"
else
  love "$wd"
fi
