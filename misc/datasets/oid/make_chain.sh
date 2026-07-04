#!/usr/bin/env sh
set -eu

# echo -e "2a5b1594c3470251\n0e7debb62c4524c0" | dash make_chain.sh /tmp/chain

# --- argument + path validation -------------------------------------------------
if [ "$#" -ne 1 ]; then
    printf 'Usage: %s <directory>\n' "$0" >&2
    exit 2
fi

# Run filter
dir="$1"

if [ ! -f "/tmp/f" ]; then
  cc filter_by_imageid.c -o "/tmp/f" -O2
fi
"/tmp/f" "$dir"

# Parse and download images

# download_images.sh — download all images listed in <dir>/images.csv in parallel.
#
# Usage: sh download_images.sh <directory>
#
# CSV format (one image per line, no header):
#   <id>,<url>
# e.g.   0,https://open-images-dataset.s3.amazonaws.com/train/2a5b1594c3470251.jpg
#
# Files are written to <directory>/<id>.jpg

echo "Downloading images"

csv="$dir/images.csv"

if [ ! -d "$dir" ]; then
    printf 'Error: not a directory: %s\n' "$dir" >&2
    exit 1
fi
if [ ! -f "$csv" ]; then
    printf 'Error: images.csv not found under %s\n' "$dir" >&2
    exit 1
fi

# --- single-image download worker ----------------------------------------------
# Args: <dir> <id> <url>
# Writes to <dir>/<id>.jpg; prints a one-line status to stderr on failure.
download_one() {
    _dir="$1"; _id="$2"; _url="$3"
    _out="$_dir/$_id.jpg"
    echo "$_out"
    if ! curl -fsSL -o "$_out" "$_url"; then
        _rc=$?
        rm -f "$_out"
        printf 'FAIL %s (curl exit %d): %s\n' "$_id" "$_rc" "$_url" >&2
        return "$_rc"
    fi
}

# --- read CSV, skip blanks/comments ---------------------
# Read line by line (handles URLs containing spaces safely), split on first comma.
while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        ''|\#*) continue ;;                 # skip blank lines and # comments
    esac
    id=${line%%,*}                          # everything before first comma
    url=${line#*,}                          # everything after first comma
    # printf '%s\t%s\t%s\n' "$dir" "$id" "$url"
    download_one "$dir" "$id" "$url"
done < "$csv"

# --- summary -------------------------------------------------------------------
total=$(grep -vc '^[[:space:]]*\(#\|$\)' "$csv" 2>/dev/null || printf 0)
printf 'Done. %d entries processed in %s\n' "$total" "$dir"
