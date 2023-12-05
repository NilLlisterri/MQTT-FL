#pragma once

#include "Arduino.h"

#include "commands/commandService.h"

#include "flMessage.h"

class FlCommandService: public CommandService {
public:
    FlCommandService();
};
