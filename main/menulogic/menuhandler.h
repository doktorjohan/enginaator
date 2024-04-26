//
// Created by johan on 25/04/2024.
//

#ifndef APP_TEMPLATE_MENUHANDLER_H
#define APP_TEMPLATE_MENUHANDLER_H
#include "../config/gameconfig.h"
#include "../config/state.h"

void handleMainMenu(struct ApplicationState *p_applicationState, struct GameConfigParameters *p_gameConfigParameters);
void handleSettings(struct ApplicationState *p_applicationState, struct GameConfigParameters *p_gameConfigParameters);
void handleLevelSelect(struct ApplicationState *p_applicationState, struct GameConfigParameters *p_gameConfigParameters);
void handleGame(struct ApplicationState *p_applicationState, struct GameConfigParameters *p_gameConfigParameters);
void handleEndGame(struct ApplicationState *p_applicationState, struct GameConfigParameters *p_gameConfigParameters);
void handleDefault(struct ApplicationState *p_applicationState, struct GameConfigParameters *p_gameConfigParameters);


#endif //APP_TEMPLATE_MENUHANDLER_H
