#! /bin/sh

PROJECT_NAME="npdemo3"
DEMO_DIR=$(mktemp -d -p /tmp ${PROJECT_NAME}.XXXX)
NP_SINGLE_DIR="$DEMO_DIR/np_single"
STUDENT_ID="$1"

chmod 700 "$DEMO_DIR"
cp -r np_single/ "$DEMO_DIR"
cp start_np_single.sh "$NP_SINGLE_DIR"
cp port.py "$NP_SINGLE_DIR"

# If the demo working directory already exists (probably because you have executed it before), remove it.
if [ -f ~/.$PROJECT_NAME ]; then
  INFO "Removing previously created demo directory."
  rm -rf "$(cat ~/.$PROJECT_NAME)"
  rm -rf "$HOME/.$PROJECT_NAME"
fi

# Create a hidden file and write the demo directory's path into it.
cat >~/.$PROJECT_NAME <<EOF
$DEMO_DIR
EOF

if [ -n "$(tmux ls | grep npdemo3)" ]; then
  tmux kill-session -t "npdemo3"
fi
tmux new-session -d -s "npdemo3"
tmux split-window -v -p 60
tmux select-pane -t 0
tmux send "cd $(dirname "$(readlink -f "$0")"); ./start_np_singles.py $NP_SINGLE_DIR" ENTER
tmux select-pane -t 1
tmux send "cd $(dirname "$(readlink -f "$0")"); ./start_http_server.sh $DEMO_DIR $STUDENT_ID" ENTER
tmux attach-session -t "npdemo3"
