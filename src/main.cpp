
#include <iostream>
#include "lua/scene_lua.hpp"

int main(int argc, char** argv)
{
  std::string filename = "assets/scenes/simple.lua";
  if (argc >= 2) {
    filename = argv[1];
  }

  if (!run_lua(filename)) {
    std::cerr << "Could not open " << filename <<
                 ". Try running the executable from inside of" <<
                 " the repository root" << std::endl;
    return 1;
  }
}
