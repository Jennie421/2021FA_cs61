//Argument Sorting

#include <cstdio>
#include <cstring>


int main(int argc, char** argv) {

  while (argc > 1) {

    int smallest = 1;

    for (int i = 2; i < argc; ++i) {
      if (strcmp(argv[i], argv[smallest]) < 0) {
        smallest = i;
      }
    }

    fprintf(stdout, "%s\n", argv[smallest]);

    argv[smallest] = argv[argc - 1];
    --argc;
  }

}
