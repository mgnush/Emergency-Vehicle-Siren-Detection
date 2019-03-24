#include <bcm2835.h>

#define PIN0 RPI_BPLUS_GPIO_J8_11
#define PIN1 RPI_BPLUS_GPIO_J8_12
#define PIN2 RPI_BPLUS_GPIO_J8_13
#define PIN3 RPI_BPLUS_GPIO_J8_15
#define PIN4 RPI_BPLUS_GPIO_J8_16

const int MAX_CYCLES = 2;

enum direction {
	approaching,
	receding,
	no_dir
};

enum location {
	south,
	west,
	east,
	north
};

void initialize_display_pins();

void update_display(const int &cycles, const location &loc, const direction &dir);
	
