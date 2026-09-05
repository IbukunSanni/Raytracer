
#include "core/Log.hpp"
#include "lua/scene_lua.hpp"

int main(int argc, char** argv)
{
  std::string filename = "assets/scenes/simple.lua";
  if (argc >= 2) {
    filename = argv[1];
  }

  if (!run_lua(filename)) {
    LOG_ERROR(LUA) << "could not open " << filename
                   << " -- run the executable from the repository root";
    return 1;
  }
}
