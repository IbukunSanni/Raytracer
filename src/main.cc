
#include "lua/scene_lua.h"

#include "core/log.h"

int main(int argc, char** argv) {
  std::string filename = "assets/scenes/simple.lua";
  if (argc >= 2) {
    filename = argv[1];
  }

  if (!RunLua(filename)) {
    LOG_ERROR(kLua) << "could not open " << filename
                    << " -- run the executable from the repository root";
    return 1;
  }
}
