#include "display.h"


void initialize_display_pins()
{
	/* pin 0 controls the middle led, connected to ground*/
	bcm2835_gpio_fsel(PIN0, BCM2835_GPIO_FSEL_OUTP);

	/* pin 1 to 4 is charlieplexed*/
	bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);	
}

void update_display(const int &cycles, const location &loc, const direction &dir)
{
	if (cycles > MAX_CYCLES) {
			// No EV
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);

			bcm2835_gpio_clr(PIN0);
	} else {
		switch(loc) {
			case north:
				switch (dir) {
					case no_dir:
						// North
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN1);
						bcm2835_gpio_clr(PIN2);
						bcm2835_gpio_clr(PIN3);
					case receding:
						// North out
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN1);
						bcm2835_gpio_clr(PIN3);
					case approaching:
						// North in
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN1);
						bcm2835_gpio_clr(PIN2);
				}
			case south:
				switch (dir) {
					case no_dir:
						// South
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN4);
						bcm2835_gpio_clr(PIN3);
						bcm2835_gpio_clr(PIN2);
					case receding:
						// South out
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN4);
						bcm2835_gpio_clr(PIN2);
					case approaching:
						// South in
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN4);
						bcm2835_gpio_clr(PIN3);
				}
			case west:
				switch (dir) {
					case no_dir:
						// West
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN3);
						bcm2835_gpio_clr(PIN1);
						bcm2835_gpio_clr(PIN2);
					case receding:
						// West out
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN3);
						bcm2835_gpio_clr(PIN1);
					case approaching:
						// West in
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN3);
						bcm2835_gpio_clr(PIN2);
				}
			case east:
				switch (dir) {
					case no_dir:
						// East
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN2);
						bcm2835_gpio_clr(PIN3);
						bcm2835_gpio_clr(PIN4);
					case receding:
						// East out
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN2);
						bcm2835_gpio_clr(PIN3);
					case approaching:
						// East in
						bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
						bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
						bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
						
						bcm2835_gpio_set(PIN0);
						bcm2835_gpio_set(PIN2);
						bcm2835_gpio_clr(PIN4);
				}
		}
	}
}
