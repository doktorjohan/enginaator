/*
**====================================================================================
** Imported definitions
**====================================================================================
*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include "driver/spi_master.h"

#include "esp_task_wdt.h"

/* Display driver is defined in display.c and display.h
 * Note that when you add new files to the project, then CMakeLists.txt also needs to be updated for these files to be built. */
#include "display.h"
/* The SD card functionality has been moved to its own separate file for this project. */
#include "sdCard.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "esp_timer.h"
#include <time.h>
#include "menulogic/menuhandler.h"
#include "config/gameconfig.h"
#include "config/state.h"

/*
**====================================================================================
** Private constant definitions
**====================================================================================
*/

#define Private static

#define PIN_NUM_CLK   12	/* SPI Clock pin */
#define PIN_NUM_MOSI  11	/* SPI Master Out, Slave in  - The ESP32 transmits data over this pin */
#define PIN_NUM_MISO  13	/* SPI Master In, Slave Out  - The ESP32 receives data over this pin. */
#define BUTTON1 45
#define BUTTON2 35
#define BUTTON3 36
#define BUTTON4 37
/* Note that additional connections are required for the display to work. These are defined in display.c */
/* PIN_NUM_DC         5  - Data/Control pin  		*/
/* PIN_NUM_RST        3  - Display Reset pin 		*/
/* PIN_NUM_CS         4  - Display Chip Select pin 	*/
/* PIN_NUM_BCKL       2  - Backlight LED 			*/
/*
 * PIN_NUM_SDCARD_CS  7  - SD Card chip select pin	*/

/* Additionally GND and 3.3V need to be connected to the GND and VCC pins on the display board respectively. */

/* Uncomment this to enable the ghost bitmap test. */
//#define GHOST_TEST

/*
**====================================================================================
** Private macro definitions
**====================================================================================
*/

#define SET_FRAME_BUF_PIXEL(buf,x,y,color) *((buf) + (x) + (320*(y)))=color

/*
**====================================================================================
** Private type definitions
**====================================================================================
*/

/*
**====================================================================================
** Private function forward declarations
**====================================================================================
*/

Private uint8_t initialize_spi(void);
Private void drawRectangleInFrameBuf(int xPos, int yPos, int width, int height, uint16_t color);
Private void drawBmpInFrameBuf(int xPos, int yPos, int width, int height, uint16_t * data_buf);
Private void switchStateTo(enum AppState toState);
#ifdef GHOST_TEST
Private void drawGhost(void);
#endif

/*
**====================================================================================
** Private variable declarations
**====================================================================================
*/
uint8_t grid[30][22];
//1 == head, 2 == keha, 3 == kurv, 4 == saba, 5 == toit
uint8_t dir = 0; //0 == right, 1 == down, 2 == left, 3 == up
int8_t xpos = 5;
int8_t ypos = 5;
bool game_ended = false;
bool just_eaten = false;
esp_timer_handle_t my_timer_handle;
bool change_alowed = false;

uint16_t * priv_frame_buffer;
uint16_t * sein;
uint16_t * muru;
uint16_t * peaR;
uint16_t * peaD;
uint16_t * peaL;
uint16_t * peaU;
uint16_t * kehaRL;
uint16_t * kehaUD;
uint16_t * sabaR;
uint16_t * sabaD;
uint16_t * sabaL;
uint16_t * sabaU;
uint16_t * curveR;
uint16_t * curveD;
uint16_t * curveL;
uint16_t * curveU;
uint16_t * toit;
uint16_t * gameover;
#define MAX_SNAKE_LENGTH 100

typedef struct {
    int x;
    int y;
} Position;

Position snake[MAX_SNAKE_LENGTH]; // Array to store snake's body positions
int snakeLength; // Current length of the snake


/*
**====================================================================================
** Public function definitions
**====================================================================================
*/
struct GameConfigParameters *p_gameConfigParameters, params;
struct ApplicationState *p_applicationState, appState;

uint16_t *get_snake_segment_image(int index) {
    if (index >= snakeLength || index < 0) return muru; // Safety check

    Position curr = snake[index];
    Position prev = { -1, -1 }, next = { -1, -1 };

    if (index > 0) prev = snake[index - 1]; // Get previous segment unless head
    if (index < snakeLength - 1) next = snake[index + 1]; // Get next segment unless tail

    // Determine head direction if the current segment is the head
    if (index == 0) {
        if (next.x == curr.x + 1) return peaL; // Head moving right
        if (next.x == curr.x - 1) return peaR; // Head moving left
        if (next.y == curr.y + 1) return peaU; // Head moving down
        if (next.y == curr.y - 1) return peaD; // Head moving up
    }

    // Determine tail direction if the current segment is the tail
    if (index == snakeLength - 1) {
        if (prev.x == curr.x + 1) return sabaR; // Tail with body to the right
        if (prev.x == curr.x - 1) return sabaL; // Tail with body to the left
        if (prev.y == curr.y + 1) return sabaD; // Tail with body above
        if (prev.y == curr.y - 1) return sabaU; // Tail with body below
    }

    // Determine body segment type
    if (prev.x != next.x && prev.y != next.y) { // Body is a curve
    	// Curve calculation based on previous and next relative positions
    	if (prev.x < curr.x && next.y > curr.y) return curveD; // Curve turning right then down
    	if (prev.x > curr.x && next.y > curr.y) return curveR; // Curve turning left then down
    	if (prev.x > curr.x && next.y < curr.y) return curveU; // Curve turning left then up
    	if (prev.x < curr.x && next.y < curr.y) return curveL; // Curve turning right then up

    	if (prev.y < curr.y && next.x > curr.x) return curveU; // Curve turning down then right
    	if (prev.y > curr.y && next.x > curr.x) return curveR; // Curve turning up then right
    	if (prev.y > curr.y && next.x < curr.x) return curveD; // Curve turning up then left
    	if (prev.y < curr.y && next.x < curr.x) return curveL; // Curve turning down then left
    } else { // Body is straight
    	if (curr.x == prev.x) return kehaUD; // Vertical body
    	if (curr.y == prev.y) return kehaRL; // Horizontal body
    }

    return muru; // Default case (should not happen in normal gameplay)
}

void init_grid(void)
{
    // Initialize all cells to 0
    for(int i = 0; i < 30; i++) {
        for(int j = 0; j < 22; j++) {
            grid[i][j] = 0;
        }
    }
}
void init_snake(void) {
    snakeLength = 5; // Initial length of snake
    snake[0].x = 1; // Head position
    snake[0].y = 0;
    snake[1].x = 0; // Initial tail position right next to head
    snake[1].y = 0;

    // Set initial positions in the grid
    grid[snake[0].x][snake[0].y] = 1; // Head
    grid[snake[1].x][snake[1].y] = 4; // Tail
}

void my_timer_callback(void* arg) {

	if(gpio_get_level(BUTTON1) == 0 && change_alowed){
		if(dir != 3 && dir != 1){
			dir = 3;
		}
		change_alowed = false;
	} else if(gpio_get_level(BUTTON2) == 0 && change_alowed){
		if(dir != 2 && dir != 0){
			dir = 2;
		}
		change_alowed = false;
	} else if(gpio_get_level(BUTTON3) == 0 && change_alowed){
		if(dir != 0 && dir != 2){
			dir = 0;
		}
		change_alowed = false;
	}else if (gpio_get_level(BUTTON4) == 0 && change_alowed) {
		if(dir != 1 && dir != 3){
			dir = 1;
		}
		change_alowed = false;
	}
}
static void IRAM_ATTR gpio_isr_handler(void* arg) {
	uint32_t io_num = (uint32_t) arg;
	if(io_num == BUTTON1){
		if(dir != 3 && dir != 1){
			dir = 3;
		}
	} else if(io_num == BUTTON2){
		if(dir != 2 && dir != 0){
			dir = 2;
		}
	} else if(io_num == BUTTON3){
		if(dir != 0 && dir != 2){
			dir = 0;
		}
	}else if (io_num == BUTTON4) {
		if(dir != 1 && dir != 3){
			dir = 1;
		}
	}
}
void place_food(void) {
    int x, y;
    srand(time(NULL));  // Seed the random number generator
    do {
        x = rand() % 30;  // Generate a random x position within the grid bounds
        y = rand() % 22;  // Generate a random y position within the grid bounds
    } while (grid[x][y] != 0);  // Ensure the position is not occupied by the snake

    grid[x][y] = 5;  // Set the grid cell to '5' to represent food
}


void move_snake(void) {
    Position newHead = snake[0];  // Get current head position

    // Calculate new head position based on current direction
    switch (dir) {
        case 0: newHead.x++; break;  // Move right
        case 1: newHead.y++; break;  // Move down
        case 2: newHead.x--; break;  // Move left
        case 3: newHead.y--; break;  // Move up
    }

    // Check for boundary collisions
    if (newHead.x < 0 || newHead.x >= 30 || newHead.y < 0 || newHead.y >= 22) {
        game_ended = true;
        return;
    }

    // Check for self-collision
    for (int i = 1; i < snakeLength; i++) {
        if (snake[i].x == newHead.x && snake[i].y == newHead.y) {
            game_ended = true;
            return;
        }
    }

    // Check if the new head position is on food
    if (grid[newHead.x][newHead.y] == 5) {
        just_eaten = true;  // Snake has just eaten
        if (snakeLength < MAX_SNAKE_LENGTH) {
            snakeLength++;  // Increase snake length
        }
    } else {
        just_eaten = false;
        // Clear the last segment of the tail from the grid if not eating
        grid[snake[snakeLength - 1].x][snake[snakeLength - 1].y] = 0;
    }

    // Move the snake's body
    for (int i = snakeLength - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    snake[0] = newHead;  // Update the head position

    // Update grid for new positions
    grid[newHead.x][newHead.y] = 1; // Head
    for (int i = 1; i < snakeLength; i++) {
        grid[snake[i].x][snake[i].y] = 2;  // Body segments
    }

    // Place new food if eaten
    if (just_eaten) {
        place_food();
    }
}




void draw_snake(void) {

	//display_drawBitmap(snake[snakeLength+1].x * 10+10, snake[snakeLength+1].y * 10+10, 10, 10, muru);
    // Redraw the entire snake
	for (int i = 0; i < snakeLength; i++) {
	    uint16_t *img = get_snake_segment_image(i);
	    display_drawBitmap(snake[i].x * 10+10, snake[i].y * 10+10, 10, 10, img);
	}
}

uint16_t* get_grid_image(int x, int y) {
    if (grid[x][y] == 5) {  // Check if the cell contains food
        return toit;
    } else {
        return muru;  // Return grass image for all other cases
    }
}
void print_grid(void) {
    for (int y = 0; y < 22; y++) {  // Assuming the grid size is 30x22
        for (int x = 0; x < 30; x++) {
            switch (grid[x][y]) {
                case 0:
                    printf(". ");  // Print a dot for empty spaces
                    break;
                case 1:
                    printf("H ");  // Print 'H' for the snake's head
                    break;
                case 2:
                    printf("B ");  // Print 'B' for the snake's body
                    break;
                case 4:
                    printf("T ");  // Print 'T' for the snake's tail
                    break;
                case 5:
                    printf("F ");  // Print 'F' for food
                    break;
                default:
                    printf("? ");  // Print '?' for an unrecognized value
            }
        }
        printf("\n");  // Newline after each row
    }
    printf("\n");  // Extra newline for separation between prints
}
void app_main(void)
{

	uint8_t res;
    params = GameConfigParameters_default;
    p_gameConfigParameters = &params;

    appState = defaultState;
    p_applicationState = &appState;

	/* Check how much RAM we have currently available... */
	printf("Total available memory: %u bytes\n", heap_caps_get_total_size(MALLOC_CAP_8BIT));

	/*Allocate memory for the frame buffer from the heap. */
    priv_frame_buffer = heap_caps_malloc(240*320*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(priv_frame_buffer);
    sein = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(sein);
    muru = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(muru);
    //pea
    peaR = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(peaR);
    peaD = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(peaD);
    peaL = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(peaL);
    peaU = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(peaU);
    //keha
    kehaRL = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(kehaRL);
    kehaUD = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(kehaUD);
    //saba
    sabaR = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(sabaR);
    sabaD = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(sabaD);
    sabaL = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(sabaL);
    sabaU = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(sabaU);
    //curve
    curveR = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(curveR);
    curveD = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(curveD);
    curveL = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(curveL);
    curveU = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(curveU);
    toit = heap_caps_malloc(10*10*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(toit);
    gameover = heap_caps_malloc(DISPLAY_WIDTH*DISPLAY_HEIGHT*sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(gameover);

	/*Call the function to initialize the SPI peripheral connected to the SD card. */
	res = initialize_spi();

	if(res == 1u)
	{
		/* Initialize the Sd Card logic as well as the display driver.
		 * Note that these devices are on the same SPI bus. The Chip Select pins allow us to
		 * select and deselect them as needed. */
		display_init();
		sdCard_init();
		init_grid();
		init_snake();
		place_food();
		sdCard_Read_bmp_file("/Sein_LR.bmp", sein);
		sdCard_Read_bmp_file("/Muru.bmp", muru);
		sdCard_Read_bmp_file("/Pea_U.bmp", peaU);
		sdCard_Read_bmp_file("/Pea_D.bmp", peaD);
		sdCard_Read_bmp_file("/Pea_R.bmp", peaR);
		sdCard_Read_bmp_file("/Pea_L.bmp", peaL);
		sdCard_Read_bmp_file("/Kere_LR.bmp", kehaRL);
		sdCard_Read_bmp_file("/Kere_UD.bmp", kehaUD);
		sdCard_Read_bmp_file("/Saba_R.bmp", sabaR);
		sdCard_Read_bmp_file("/Saba_L.bmp", sabaL);
		sdCard_Read_bmp_file("/Saba_U.bmp", sabaU);
		sdCard_Read_bmp_file("/Saba_D.bmp", sabaD);
		sdCard_Read_bmp_file("/Kurv_RD.bmp", curveR);
		sdCard_Read_bmp_file("/Kurv_LD.bmp", curveD);
		sdCard_Read_bmp_file("/Kurv_RU.bmp", curveU);
		sdCard_Read_bmp_file("/Kurv_LU.bmp", curveL);
		sdCard_Read_bmp_file("/apple.bmp", toit);
		sdCard_Read_bmp_file("/GameOver.bmp", gameover);
		for(int i = 0; i < 32; i++) {
			for(int j = 0; j < 24; j++) {
				if(i == 0 || i == 31 || j == 0 || j == 23) {
					display_drawBitmap(i*10, j*10, 10, 10, sein);
				}
			}
		}
		gpio_config_t io_conf;
		io_conf.intr_type = GPIO_INTR_NEGEDGE;  // Trigger on negative edge for button press
		io_conf.mode = GPIO_MODE_INPUT;
		io_conf.pin_bit_mask = (1ULL << BUTTON1) | (1ULL << BUTTON2) | (1ULL << BUTTON3) | (1ULL << BUTTON4);
		io_conf.pull_down_en = 0;  // Not enabling pull-down resistor
		io_conf.pull_up_en = 1;    // Enable pull-up resistor
		gpio_config(&io_conf);
		const esp_timer_create_args_t my_timer_args = {
				.callback = &my_timer_callback,
				.name = "my_timer" // Optional: name of the timer
		};

		// Create the timer
		esp_timer_create(&my_timer_args, &my_timer_handle);

		// Start the timer with a period of one second (1,000,000 microseconds)
		esp_timer_start_periodic(my_timer_handle, 1000);
		/*
		gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);  // Use appropriate interrupt flag

		gpio_isr_handler_add(BUTTON1, gpio_isr_handler, (void*) BUTTON1);
		gpio_isr_handler_add(BUTTON2, gpio_isr_handler, (void*) BUTTON2);
		gpio_isr_handler_add(BUTTON3, gpio_isr_handler, (void*) BUTTON3);
		gpio_isr_handler_add(BUTTON4, gpio_isr_handler, (void*) BUTTON4);
*/


		//vTaskDelay(1000u / portTICK_PERIOD_MS);

		/* Load an image from the SD Card into the frame buffer */
		//sdCard_Read_bmp_file("/logo.bmp", priv_frame_buffer);

		//display_drawBitmap(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, priv_frame_buffer);
	}

	/* Five second delay... */
	vTaskDelay(500u / portTICK_PERIOD_MS);

	/* The idea is that we will try to keep a cyclic process that is called every 40 milliseconds, we call our
	 * drawing functions and then delay for the period remaining.
	 */

	TickType_t xLastWakeTime;
	const TickType_t xFrequency = 40u / portTICK_PERIOD_MS;
	xLastWakeTime = xTaskGetTickCount ();

	/* Main CPU cycle */
	int first = 0;
	while(1)
	{

        switch (p_applicationState->menuState) {
            case MAIN_MENU:
                handleMainMenu(p_applicationState, p_gameConfigParameters);
                break;
            case SETTINGS:
                handleSettings(p_applicationState, p_gameConfigParameters);
                break;
            case LEVEL_SELECT:
                handleLevelSelect(p_applicationState, p_gameConfigParameters);
                break;
            case GAME:
                handleGame(p_applicationState, p_gameConfigParameters);
                break;
            case END_GAME:
                handleEndGame(p_applicationState, p_gameConfigParameters);
                break;
            default:
                handleDefault(p_applicationState, p_gameConfigParameters);
                break;
        }

		//print_grid();
		if(!game_ended){
			for(int i = 0; i < 30; i++) {
				for(int j = 0; j < 22; j++) {
					display_drawBitmap(i*10+10, j*10+10, 10, 10, get_grid_image(i, j));

				}
			}
			draw_snake();
			move_snake();
			change_alowed = true;
		}
		if(game_ended && first == 0){
			printf("ended\n");
			display_drawBitmap(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, gameover);
			for(int i = 0; i < 32; i++) {
				for(int j = 0; j < 24; j++) {
					if(i == 0 || i == 31 || j == 0 || j == 23) {
						display_drawBitmap(i*10, j*10, 10, 10, sein);
					}
				}
			}
			first = 1;
		}
		vTaskDelay(100u / portTICK_PERIOD_MS);
		vTaskDelayUntil( &xLastWakeTime, xFrequency );
	}
}

/*
**====================================================================================
** Private function definitions
**====================================================================================
*/

/* This function initializest the SPI interface, but does not yet add any devies on it.  */
Private uint8_t initialize_spi(void)
{
    esp_err_t ret;

    printf("Setting up SPI peripheral\n");

    spi_bus_config_t bus_cfg =
    {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_MAX_TRANSFER_SIZE,
    };

    ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);

    if (ret != ESP_OK)
    {
        printf("Failed to initialize bus.\n");
        return 0u;
    }

    return 1u;
}


Private void drawRectangleInFrameBuf(int xPos, int yPos, int width, int height, uint16_t color)
{
	for (int x = xPos; ((x < (xPos+width)) && (x < 320)); x++)
	{
		for (int y = yPos; ((y < (yPos+height)) && (y < 240)); y++)
		{
			SET_FRAME_BUF_PIXEL(priv_frame_buffer, x, y, color);
		}
	}
}


Private void drawBmpInFrameBuf(int xPos, int yPos, int width, int height, uint16_t * data_buf)
{
	uint16_t * data_ptr = data_buf;

	for (int x = xPos; (x < (xPos + width)); x++)
	{
		for (int y = yPos; (y < (yPos + height)); y++)
		{
			if ((x >= 0) && (x < DISPLAY_WIDTH) && (y >= 0) && (y < DISPLAY_HEIGHT))
			{
				SET_FRAME_BUF_PIXEL(priv_frame_buffer, x, y, *data_ptr);
			}
			data_ptr++;
		}
	}
}

Private void switchStateTo(enum AppState toState)
{
    p_applicationState->menuState = toState;
}

#ifdef GHOST_TEST
Private void drawGhost(void)
{
	/* Here we draw the whole screen buffer every time. Note that this is not necessarily the most optimal solution, since it takes a lot
	 * of time to redraw the whole screen */

	/* Update cube position */
	ghost_position += ghost_direction;

	if(ghost_position <= 0)
	{
		ghost_direction = GHOST_SPEED;
	}

	if(ghost_position >= (DISPLAY_WIDTH - 64))
	{
		ghost_direction = 0 - GHOST_SPEED;
	}

	/* Here we draw the data onto an internal buffer and then pass the whole buffer on to the display driver. */
	/* 1. Draw the background in the buffer */
	drawRectangleInFrameBuf(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, COLOR_WHITE);
	/* 2. Draw the ghost bitmap in the buffer */
	drawBmpInFrameBuf(ghost_position, 88, 64, 64, priv_ghost_buffer);

	/* 3. Flush the frame buffer. */
	display_drawScreenBuffer(priv_frame_buffer);
}
#endif

