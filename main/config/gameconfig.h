#ifndef APP_TEMPLATE_GAMECONFIG_H
#define APP_TEMPLATE_GAMECONFIG_H

#include <stdlib.h>

enum Level {
    REGULAR,
    ENGINAATOR,
    OSC
};

struct GameConfigParameters {
    uint8_t snakeSpeedIncrement;
    uint8_t snakeSpeed;
    uint8_t difficulty;
    enum Level level;
} GameConfigParameters_default = {1, 1, 1, REGULAR};

#endif //APP_TEMPLATE_GAMECONFIG_H
