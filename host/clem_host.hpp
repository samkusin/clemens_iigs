#ifndef CLEM_HOST_HPP
#define CLEM_HOST_HPP

#include "emulator.h"
#include "cinek/fixedstack.hpp"
#include <cstdint>

class ClemensHost
{
public:
  ClemensHost();
  ~ClemensHost();

  void frame(int width, int height, float deltaTime);

private:
  bool parseCommand(const char* buffer);
  bool parseCommandPower(const char* line);
  bool parseCommandReset(const char* line);
  bool parseCommandStep(const char* line);

  void createMachine();
  void destroyMachine();
  void resetMachine();

private:
  ClemensMachine machine_;
  cinek::FixedStack slab_;

  int emulationStepCount_;
};


#endif
