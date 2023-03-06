//Argument Sorting using C++ library features
/*
vector.push_back(object): Adds a new element at the end of the vector
std::sort(vect.begin(),vect.end()): sort vector
std::cout << i << 'xxx': 按顺序打印
vect.size()
*/

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>


int main(int argc, char** argv) {
  // create a vector of the argument strings
  // (argv[0] holds the program name, which doesn’t count)
  std::vector<std::string> args;
  for (int i = 1; i != argc; ++i) {
    args.push_back(argv[i]);
  }

  // sort the vector’s contents
  std::sort(args.begin(), args.end());

  // print the vector’s contents
  for (int i = 0; i != args.size(); ++i) {
    std::cout << args[i] << '\n';
  }

}
