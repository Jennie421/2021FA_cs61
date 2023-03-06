//unix cat
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

static void transfer(const char* filename) {
  //open input file
  FILE* in;
  if (strcmp(filename, "-") == 0) { //string comparison, if same then == 0
    in = stdin;
  } else {
    in = fopen(filename, "r");
  }
  if (!in) {
    fprintf(stderr, "%s: %s\n", filename, strerror(errno));
    exit(1);
  }

  //transfer data until EOF or error
  while (!feof(in) && !ferror(in) && !ferror(stdout)) {
    char buf[BUFSIZ];
    size_t nr = fread(buf, 1, BUFSIZ, in);
    (void) fwrite(buf, 1, nr, stdout);
  }

  //exit on error
  if (ferror(in) || ferror(stdout)) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }

  //close input file if required
  if (in != stdin) {
    fclose(in);
  }

}


int main(int argc, char** argv) {
  if (argc == 1) {
    transfer("-");
  }

  for (int i = 1; i < argc; ++i){
    transfer(argv[i]);
  }
}

//$ c++ -std=gnu++1z -Wall -g -O3 cat61.cc -o cat61
