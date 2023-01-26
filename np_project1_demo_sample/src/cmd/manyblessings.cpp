#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <string>
using namespace std;
int main() {
  string blessing = "We wish you the best!";
  for (int i = 0; i < 5000; i++) {
    cout << blessing << endl;
  }
  return 0;
}

