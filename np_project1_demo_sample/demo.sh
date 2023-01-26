#! /bin/sh

SHELL_PATH=$1

if [ -z ${SHELL_PATH} ] || [ ! -x ${SHELL_PATH} ]; then
  echo "Usage: $0 [shell_path]"
  exit 1
fi

SHELL_PATH=$( readlink -f ${SHELL_PATH} )

TEST_CASE_START=1

[ -n "$2" ] && TEST_CASE_START=$2

mkdir -p output
gmake clean

for i in $( seq ${TEST_CASE_START} 7 ); do
  gmake
  echo "[1;34m===== Test case ${i} =====[m"
  cd work_dir
  rm -f ../output/${i}.txt
  env -i stdbuf -o 0 -e 0 ${SHELL_PATH} < ../test_case/${i}.txt > ../output/${i}.txt 2>&1
  cd ..
  diff -w output/${i}.txt answer/${i}.txt > /dev/null
  if [ $? -eq 0 ]; then
    echo "Your answer is [0;32mcorrect[m"
    correct_cases="${correct_cases} ${i}"
  else
    echo "Your answer is [0;31mwrong[m"
    wrong_cases="${wrong_cases} ${i}"
  fi
done

echo "[1;34m======= Summary =======[m"
[ -n "${correct_cases}" ] && echo "[0;32m[Correct][m:${correct_cases}"
[ -n "${wrong_cases}" ] && echo "[0;31m[ Wrong ][m:${wrong_cases}"
