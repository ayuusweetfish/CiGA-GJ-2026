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

# Download images.csv using curl --parallel in one command.
# Each CSV line: <index>,<URL>. Outputs are $dir/<index>.jpg.

csvfile="$dir/images.csv"

if [ ! -f "$csvfile" ]; then
  echo "Error: $csvfile not found" >&2
  exit 1
fi

(
# Change to the output directory so filenames are simple (0.jpg, 1.jpg, ...)
cd "$dir" || exit 1

# Build a command string: curl --parallel -o INDEX.jpg URL -o INDEX.jpg URL ...
# Shell-escape each argument with single quotes; wrap single quotes in '"'"'".
quote() {
  printf '%s\n' "$1" | sed -e "s/'/'\\\\''/g" -e "1s/^/'/" -e "\$s/\$/'/"
}

# Initialize the args variable with curl --parallel
args=
args="${args}curl --parallel "

while IFS= read -r line || [ -n "$line" ]; do
  # Skip empty lines
  [ -z "$line" ] && continue

  # Trim surrounding whitespace
  line=$(printf '%s\n' "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

  # Split on first comma: idx and url
  idx="${line%%,*}"
  url="${line#*,}"

  if [ -z "$idx" ] || [ -z "$url" ]; then
    printf 'Malformed line (ignored): %s\n' "$line" >&2
    continue
  fi

  # Append -o "$idx.jpg" and "$url", each properly quoted
  out="${idx}.jpg"
  args="${args}$(quote '-o') $(quote "$out") $(quote "$url") "
done < images.csv

# Remove the trailing space from the last appended chunk
args="${args% }"

# If there are no valid entries, bail out
case "$args" in
  "curl --parallel") echo "No valid entries in images.csv" >&2; exit 1 ;;
esac

# Run the entire curl command in one invocation
eval "$args"
)
