/*

file    :  src/app-proxy.cpp
repo    :  https://github.com/fuzzah/exeptor
author  :  https://github.com/fuzzah
license :  MIT
check repository for more information

app-proxy - proxy app to start pass LD_PRELOAD to scripts

*/

#include <iostream>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cout << argv[0] << " - proxy for exec-family syscalls\n";
    std::cout << "Run it like this: " << argv[0] << " some-program"
              << std::endl;
    return 0;
  }

  std::cout << "Trying to run via exec:";
  for (int i = 1; i < argc; i++) {
    std::cout << " " << argv[i];
  }
  std::cout << std::endl;

  if (-1 == execvp(argv[1], &argv[1])) {
    perror("Wasn't able to run the program specified");
    _exit(1);
  }

  return 0;
}
