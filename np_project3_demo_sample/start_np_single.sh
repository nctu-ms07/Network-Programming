#! /bin/sh

cleanup() {
  rm -rf "$NP_SINGLE_DIR/$WORKING_DIR"
}

PORT=$(python3 port.py)
NP_SINGLE_DIR="$1"
WORKING_DIR="working_dir_${PORT}"

cd "$NP_SINGLE_DIR" || exit
cp -r "working_dir_template/" "$WORKING_DIR"

trap cleanup 0 1 2 3 6

if cd "$WORKING_DIR"; then
  if [ -x ./np_single_golden ]; then
    echo "A chat room (np_single_golden) is listening on \"$(hostname)\" and port \"$PORT\""
    ./np_single_golden "$PORT"
  else
    echo "Either np_single_golden cannot be found or it is not an executable..."
    exit 1
  fi
else
  echo "Cannot change to the directory \"/tmp/np_single/$WORKING_DIR\""
  exit 1
fi
