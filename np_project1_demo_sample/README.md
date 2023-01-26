# NP Project1 Demo Script

### Important information: 
This is a sample demo script. It is just a sample, the final one might be different.

### This directory contains the following
- demo.sh: 
    - Usage: `./demo.sh [npshell_path]`
    - demo.sh does the following:
        1. Contruct the working directory (work_template and work_dir)
        2. Compile the commands (noop, removetag...) and place them into bin/ inside the working directory
        3. Copy ls, cat, and wc into bin/ inside the working directory
        4. Run the npshell inside work_dir
        5. Redirect stdin of npshell to files in test_case/
        6. Redirect stdout and stderr of npshell to files in output/
        7. Use diff to compare the files inside output/ and answer/
        8. Show the result of demo

- compare.sh
    - Usage: `./compare.sh [n]`
    - Compare.sh will run vimdiff on the n'th answer and your output

- test_case/
    - Contains test cases

- answer/
    - Contains answers

- src/
    - Contains source code of commands (noop, removetag...) and test.html

### Scenario
- Assume there are two directories, np_project1_demo_sample and 309554042 inside a private directroy on NP server.
- My npshell is inside 309554042 directory in the home directory
- I am now inside np_project1_demo_sample

```
  bash$ ./demo.sh ~/309554042/npshell
  ...
  ... # Some messeges generated while compiling and setting up environment
  ...
  ===== Test case 1 =====
  Your answer is correct
  ===== Test case 2 =====
  Your answer is correct
  ===== Test case 3 =====
  Your answer is correct
  ===== Test case 4 =====
  Your answer is correct
  ===== Test case 5 =====
  Your answer is correct
  ===== Test case 6 =====
  Your answer is wrong
  ===== Test case 7 =====
  Your answer is correct
  ======= Summary =======
  
  [Correct]: 1 2 3 4 5 7
  [ Wrong ]: 6
  bash$ ./compare 6     # Check out the difference between the 6th output and answer
```

