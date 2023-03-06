//mystrstr using array notation[]
//我的方法
#include <cstring>
#include <cassert>
#include <cstdio>


char* mystrstr(const char* s1, const char* s2) {

    if (s2[0] == 0) {
      return (char*)s1; // return type必须和function type一致
    }

    for (int j = 0; s1[j] != 0; j++) {
      for (int i = 0; s2[i] == s1[j]; i++) {
        int size = j;
        const char* p;
        while (s2[i] == s1[j]) {
          if (s2[i] != 0) {
            p = s1 + size;
            i++;
            j++;
          }
          else {
            return (char*)p;
          }

        }
      }
    }
    return nullptr;
}

int main(int argc, char** argv) {
    assert(argc == 3);
    printf("strstr(\"%s\", \"%s\") = %p\n",
           argv[1], argv[2], strstr(argv[1], argv[2]));
    printf("mystrstr(\"%s\", \"%s\") = %p\n",
           argv[1], argv[2], mystrstr(argv[1], argv[2]));
    assert(strstr(argv[1], argv[2]) == mystrstr(argv[1], argv[2]));
}
