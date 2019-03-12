#include <bcm2835.h>

#define PIN0 RPI_GPIO_P1_0
#define PIN1 RPI_GPIO_P1_1
#define PIN2 RPI_GPIO_P1_2
#define PIN3 RPI_GPIO_P1_3
#define PIN4 RPI_GPIO_P1_4

//https://elinux.org/RPi_GPIO_Code_Samples
int initialize_pins (void)
{
	/* not sure if needed	
	if (!bcm2835_init())
	{
		printf("display initiasation failed \n");
		return 1;
	}	
	*/
	/* pin 0 controls the middle led, connected to ground*/
	bcm2835_gpio_fsel(PIN0, BCM2835_GPIO_FSEL_OUTP);

	/* pin 1 to 4 is charlieplexed*/
	bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
	bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);	
	
	return 0;
}

int display (state direction)
{
	switch(direction)
	{
		case no_ev:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);

			bcm2835_gpio_clr(PIN0);

		/* next 8 state turn on 1 led */
		case north_in:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN1);
			bcm2835_gpio_clr(PIN2);

		case north_out:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN1);
			bcm2835_gpio_clr(PIN3);
		case south_in:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN4);
			bcm2835_gpio_clr(PIN3);
		case south_out:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN4);
			bcm2835_gpio_clr(PIN2);

		case west_in:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN3);
			bcm2835_gpio_clr(PIN2);

		case west_out:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN3);
			bcm2835_gpio_clr(PIN1);

		case east_in:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN2);
			bcm2835_gpio_clr(PIN4);

		case east_out:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN2);
			bcm2835_gpio_clr(PIN3);

		/* next 4 state turn on 2 leds at a time 
		to signify location, but not direction*/
		case north:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN1);
			bcm2835_gpio_clr(PIN2);
			bcm2835_gpio_clr(PIN3);

		case south:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN4);
			bcm2835_gpio_clr(PIN3);
			bcm2835_gpio_clr(PIN2);

		case east:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_INPT);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_OUTP);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN2);
			bcm2835_gpio_clr(PIN3);
			bcm2835_gpio_clr(PIN4);

		case west:
			bcm2835_gpio_fsel(PIN1, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN2, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN3, BCM2835_GPIO_FSEL_OUTP);
			bcm2835_gpio_fsel(PIN4, BCM2835_GPIO_FSEL_INPT);
			
			bcm2835_gpio_set(PIN0);
			bcm2835_gpio_set(PIN3);
			bcm2835_gpio_clr(PIN1);
			bcm2835_gpio_clr(PIN2);

	}
	return 0;
}