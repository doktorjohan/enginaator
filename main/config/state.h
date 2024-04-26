#ifndef APP_TEMPLATE_STATE_H
#define APP_TEMPLATE_STATE_H

enum AppState {
    MAIN_MENU,
    SETTINGS,
    LEVEL_SELECT,
    GAME,
    END_GAME
};

struct ApplicationState {
    enum AppState menuState;
} defaultState = {MAIN_MENU};

#endif //APP_TEMPLATE_STATE_H
