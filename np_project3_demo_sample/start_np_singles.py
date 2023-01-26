#! /usr/bin/env python3
import random
import getpass
import subprocess
import time
import sys


class ChatRoomsController:
    USERNAME = getpass.getuser()

    @staticmethod
    def create():
        NP_SINGLE_DIR = sys.argv[1]
        OPEN_CHAT_ROOM_CMD = f'{NP_SINGLE_DIR}/start_np_single.sh {NP_SINGLE_DIR}'

        for i in range(3):
            subprocess.Popen(OPEN_CHAT_ROOM_CMD,
                             shell=True,
                             stderr=subprocess.DEVNULL)

        time.sleep(1)

    @staticmethod
    def delete():
        KILL_CHAT_ROOMS_CMD = "killall -u {} np_single_golden"
        subprocess.check_call(
            KILL_CHAT_ROOMS_CMD.format(ChatRoomsController.USERNAME),
            shell=True,
            stdout=subprocess.DEVNULL)


if __name__ == '__main__':
    try:
        ChatRoomsController.create()
        input("(Press any key (or Ctrl+C) to close...)")
        print("\nClosing chat rooms...")
        ChatRoomsController.delete()
    except KeyboardInterrupt:
        ...
