# NP Project2 Demo Script
## Important information:

1. This is a sample demo scipt. It is just a sample, the final one might be different.

2. The test cases in this directory are from 2016, the ones used for this year will be different.

3. Please make your servers fit the requirements:
   - yourNpServer takes in 1 argument, which is the port number.

   `./yourNpServer 12345`

   - There will be a user_pipe/ directory inside the working directory. For concurrent connection-oriented paradigm with shared memory, user pipes (fifo files) should be saved inside yourWorkingDir/user_pipe/

   - The initial working directory structure:

```
    work_dir
        |-bin
        |-test.html
        |-user_pipe
```

4. You can use the port that './port.py' outputs to test `reuseaddr` function.

5. We recommand you use telnet during delopment.
   - Assume you run your server on nplinux2 listening to port 12345
   - Run the following telnet commnad on any computer

```
    bash$ telnet nplinux2.cs.nctu.edu.tw 12345
    Trying 140.113.235.217...
    Connected to nplinux2.cs.nctu.edu.tw.
    Escape character is '^]'.
    ****************************************
    ** Welcome to the information server. **
    ****************************************
    *** User '(no name)' entered from CGILAB/511. ***
    %
```

6. Please be aware that the line sent from telnet and our delayclient might end with "\r\n".

7. The output order of the command "ls" might be different, we will try to avoid this situation during demo.


## This directory contains the following

### Server

- server.sh
    - Usage: `./server.sh <server_path> <port>`
    - server.sh does the following:
        1. Contruct the working directory (work_template and work_dir)
        2. Compile the commands (noop, removetag...) and place them into bin/ inside the working directory
        3. Copy cat and ls into bin/ inside the working directory
        4. Create working dir
        5. Run the your server inside working dir

- zombie.sh
    - Check if your server left any zombie processes in the system
    - Run this script right before you shut down your server

- shm.sh
    - Check if your server left shared memory in the system
    - Run this script after you shut down your server

- src/
    - Contains source code of commands (noop, removetag...) and test.html

### Client

- There are three directories inside client: single_client and multi_client
    - Use single_client to test the first server (np_simple)
    - Use multi_client_1 to test the second server (np_single_proc)
    - Use multi_client_2 to test the third server (np_multi_proc)
    

- demo.sh:
    - Usage: `./demo.sh <server ip> <port>`
    - demo.sh does the following:
        1. Test all testcases using delayclient
        2. Use diff to compare the files inside output/ and answer/
        3. Show the result of demo

- compare.sh
    - Usage: `./compare.sh [n]`
    - Compare.sh will run vimdiff on the n'th answer and your output

- test_case/
    - Contains test cases

- answer/
    - Contains answers

## Usage

### Tmux usage

- create tmux: `tmux`
- enter exist tmux session: `tmux a`
- exit but not kill session: `Ctrl-b + d`
- create new window: `Ctrl-b + c`
- next window: `Ctrl-b + n`
- kill window: `Ctrl-b + &`
- exit a pane: `Ctrl-d` (send EOF to shell)
- scroll screen: `Ctrl-b + [` (quit with q)
- go to window up: `Ctrl-b + <up>`

### Testing all servers

```
./demo.sh <project_path> <port1> <port2> <port3>
```
The script will create a new window and run all servers with corresponding testcases. You can restart servers on right three panes, and test again on the left three panes.

```
clear                                                            │rm -rf work_dir
[yywu7856443@nplinux12 ~]$ clear                                 │rm -rf work_template
[yywu7856443@nplinux12 ~]$ cd /u/gcs/108/0856022/np_project2_demo│mkdir work_template
_sample/client/single_client                                     │cp src/file/test.html work_template/test.html
[yywu7856443@nplinux12 single_client]$ ./demo.sh 127.0.0.1 12344 │mkdir work_template/bin/
===== Test case 1 =====                                          │clang++ src/cmd/number.cpp -Wall -O2 -pedantic -std=c++11 -o wor
Your answer is correct                                           │k_template/bin/number
===== Test case 2 =====                                          │clang++ src/cmd/removetag.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                           │work_template/bin/removetag
===== Test case 3 =====                                          │clang++ src/cmd/noop.cpp -Wall -O2 -pedantic -std=c++11 -o work_
Your answer is correct                                           │template/bin/noop
======= Summary =======                                          │clang++ src/cmd/removetag0.cpp -Wall -O2 -pedantic -std=c++11 -o
[Correct]: 1 2 3                                                 │ work_template/bin/removetag0
[yywu7856443@nplinux12 single_client]$                           │cp -f /bin/ls work_template/bin/ls
                                                                 │cp -f /bin/cat work_template/bin/cat
                                                                 │mkdir work_template/user_pipe
                                                                 │======= Your server is running =======
                                                                 │
─────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────
Your answer is correct                                           │mkdir work_template
===== Test case 2 =====                                          │cp src/file/test.html work_template/test.html
Your answer is correct                                           │mkdir work_template/bin/
===== Test case 3 =====                                          │clang++ src/cmd/number.cpp -Wall -O2 -pedantic -std=c++11 -o wor
Your answer is correct                                           │k_template/bin/number
===== Test case 4 =====                                          │clang++ src/cmd/removetag.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                           │work_template/bin/removetag
===== Test case 5 =====                                          │clang++ src/cmd/noop.cpp -Wall -O2 -pedantic -std=c++11 -o work_
Your answer is correct                                           │template/bin/noop
===== Test case 6 =====                                          │clang++ src/cmd/removetag0.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                           │ work_template/bin/removetag0
===== Test case 7 =====                                          │cp -f /bin/ls work_template/bin/ls
Your answer is correct                                           │cp -f /bin/cat work_template/bin/cat
======= Summary =======                                          │mkdir work_template/user_pipe
[Correct]: 1 2 3 4 5 6 7                                         │======= Your server is running =======
[yywu7856443@nplinux12 multi_client_1]$                          │
─────────────────────────────────────────────────────────────────┼────────────────────────────────────────────────────────────────
Your answer is correct                                           │mkdir work_template
===== Test case 2 =====                                          │cp src/file/test.html work_template/test.html
Your answer is correct                                           │mkdir work_template/bin/
===== Test case 3 =====                                          │clang++ src/cmd/number.cpp -Wall -O2 -pedantic -std=c++11 -o wor
Your answer is correct                                           │k_template/bin/number
===== Test case 4 =====                                          │clang++ src/cmd/removetag.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                           │work_template/bin/removetag
===== Test case 5 =====                                          │clang++ src/cmd/noop.cpp -Wall -O2 -pedantic -std=c++11 -o work_
Your answer is correct                                           │template/bin/noop
===== Test case 6 =====                                          │clang++ src/cmd/removetag0.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                           │ work_template/bin/removetag0
===== Test case 7 =====                                          │cp -f /bin/ls work_template/bin/ls
Your answer is correct                                           │cp -f /bin/cat work_template/bin/cat
======= Summary =======                                          │mkdir work_template/user_pipe
[Correct]: 1 2 3 4 5 6 7                                         │======= Your server is running =======
[yywu7856443@nplinux12 multi_client_2]$                          │
```

### Testing one server

```
./demo.sh <server_path> <port>

example:
./demo.sh ../np_simple 12345
./demo.sh ../np_single_proc 12345
```

Create two panes and run server with corresponding testcases. You can `cd` into corresponding client and run `compare.sh` on the top pane.

```
[yywu7856443@nplinux12 ~]$















────────────────────────────────────────────────────────────────────┬────────────────────────────────────────────────────────────────
[yywu7856443@nplinux12 multi_client_1]$ ./demo.sh 127.0.0.1 12344   │rm -rf work_dir
===== Test case 1 =====                                             │rm -rf work_template
Your answer is correct                                              │mkdir work_template
===== Test case 2 =====                                             │cp src/file/test.html work_template/test.html
Your answer is correct                                              │mkdir work_template/bin/
===== Test case 3 =====                                             │clang++ src/cmd/number.cpp -Wall -O2 -pedantic -std=c++11 -o wor
Your answer is correct                                              │k_template/bin/number
===== Test case 4 =====                                             │clang++ src/cmd/removetag.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                              │work_template/bin/removetag
===== Test case 5 =====                                             │clang++ src/cmd/noop.cpp -Wall -O2 -pedantic -std=c++11 -o work_
Your answer is correct                                              │template/bin/noop
===== Test case 6 =====                                             │clang++ src/cmd/removetag0.cpp -Wall -O2 -pedantic -std=c++11 -o
Your answer is correct                                              │ work_template/bin/removetag0
===== Test case 7 =====                                             │cp -f /bin/ls work_template/bin/ls
Your answer is correct                                              │cp -f /bin/cat work_template/bin/cat
======= Summary =======                                             │mkdir work_template/user_pipe
[Correct]: 1 2 3 4 5 6 7                                            │======= Your server is running =======
[yywu7856443@nplinux12 multi_client_1]$                             │
```



