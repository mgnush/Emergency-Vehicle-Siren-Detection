#include <wiringPi.h>

int initialize_pins (void)
{
	wiringPiSetup();
	/* pin 0 controls the middle led, connected to ground*/
	pinMode(0, OUTPUT);

	/* pin 1 to 4 is charlieplexed*/
	pinMode(1, INPUT);
	pinMode(2, INPUT);
	pinMode(3, INPUT);
	pinMode(4, INPUT);
	
	
	return 0;
}

int display (state direction)
{
	switch(direction)
	{
		case no_ev:
			pinMode(1, INPUT);
			pinMode(2, INPUT);
			pinMode(3, INPUT);
			pinMode(4, INPUT);

			digitalWrite(0, LOW);

		/* next 8 state turn on 1 led */
		case north_in:
			pinMode(1, OUTPUT);
			pinMode(2, OUTPUT);
			pinMode(3, INPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(1, HIGH);
			digitalWrite(2, LOW);

		case north_out:
			pinMode(1, OUTPUT);
			pinMode(2, INPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(1, HIGH);
			digitalWrite(3, LOW);
		case south_in:
			pinMode(1, INPUT);
			pinMode(2, INPUT);
			pinMode(3, OUTPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(4, HIGH);
			digitalWrite(3, LOW);
		case south_out:
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, INPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(4, HIGH);
			digitalWrite(2, LOW);

		case west_in:
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(3, HIGH);
			digitalWrite(2, LOW);

		case west_out:
			pinMode(1, OUTPUT);
			pinMode(2, INPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(3, HIGH);
			digitalWrite(1, LOW);

		case east_in:
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, INPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(2, HIGH);
			digitalWrite(4, LOW);

		case east_out:
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(2, HIGH);
			digitalWrite(3, LOW);

		/* next 4 state turn on 2 leds at a time 
		to signify location, but not direction*/
		case north:
			pinMode(1, OUTPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(1, HIGH);
			digitalWrite(2, LOW);
			digitalWrite(3, LOW);

		case south:
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(4, HIGH);
			digitalWrite(3, LOW);
			digitalWrite(2, LOW);

		case east:
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(2, HIGH);
			digitalWrite(3, LOW);
			digitalWrite(4, LOW);

		case west:
			pinMode(1, OUTPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(3, HIGH);
			digitalWrite(1, LOW);
			digitalWrite(2, LOW);

	}
	return 0;
}
	
