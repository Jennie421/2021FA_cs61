#include <cstdio> //等同 <stdio.h>
#include <ctype.h> //isspace

using namespace std;

int main() {

  unsigned long byteCount = 0, wordCount = 0, lineCount = 0;
  bool inspace = true;

  //stdin is pointer, EOF is int
  while (true) {
    int ch = fgetc(stdin); //fgetc returns an int, either char or EOF
    if(ch == EOF) {
      break;
    }
    ++byteCount;

    bool thisspace = isspace((unsigned char) ch);
    if (inspace && !thisspace) {
      ++wordCount;
    }
    inspace = thisspace;

    if (ch == '\n') {
      ++lineCount;
    }
  }


  fprintf(stdout, "%8lu %7lu %7lu\n", lineCount, wordCount, byteCount); // lu = long unsigned

  return 0;
}

//每次运行前需compile
//$ c++ -std=gnu++1z -Wall -g -O3 wc61.cc -o wc61
