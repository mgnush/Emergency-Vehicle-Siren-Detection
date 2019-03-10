#include <wiringPi.h>

enum state {
	no_vehicle,
	north_incoming,
	north_outgoing,
	south_incoming,
	south_outgoing,
	west_incoming,
	west_outgoing,
	east_incoming,
	east_outgoing,
	north,
	south, 
	east,
	west
};


int initialize_pins (void);
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

int dispay (state direction)
{
	switch(direction)
	{
		case 'no_vehicle':
			pinMode(1, INPUT);
			pinMode(2, INPUT);
			pinMode(3, INPUT);
			pinMode(4, INPUT);

			digitalWrite(0, LOW);

		/* next 8 state turn on 1 led */
		case 'north_incoming':
			pinMode(1, OUTPUT);
			pinMode(2, OUTPUT);
			pinMode(3, INPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(1, HIGH);
			digitalWrite(2, LOW);

		case 'north_outgoing':
			pinMode(1, OUTPUT);
			pinMode(2, INPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(1, HIGH);
			digitalWrite(3, LOW);
		case 'south_incoming':
			pinMode(1, INPUT);
			pinMode(2, INPUT);
			pinMode(3, OUTPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(4, HIGH);
			digitalWrite(3, LOW);
		case 'south_outgoing':
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, INPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(4, HIGH);
			digitalWrite(2, LOW);

		case 'west_incoming':
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(3, HIGH);
			digitalWrite(2, LOW);

		case 'west_outgoing':
			pinMode(1, OUTPUT);
			pinMode(2, INPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(3, HIGH);
			digitalWrite(1, LOW);

		case 'east_incoming':
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, INPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(2, HIGH);
			digitalWrite(4, LOW);

		case 'east_outgoing':
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(2, HIGH);
			digitalWrite(3, LOW);

		/* next 4 state turn on 2 leds at a time 
		to signify location, but not direction*/
		case 'north';
			pinMode(1, OUTPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, INPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(1, HIGH);
			digitalWrite(2, LOW);
			digitalWrite(3, LOW);

		case 'south':
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(4, HIGH);
			digitalWRite(3, LOW);
			digitalWrite(2, LOW);

		case 'east':
			pinMode(1, INPUT);
			pinMode(2, OUTPUT);
			pinMode(3, OUTPUT);
			pinMode(4, OUTPUT);
			
			digitalWrite(0, HIGH);
			digitalWrite(2, HIGH);
			digitalWrite(3, LOW);
			digitalWrite(4, LOW);

		case 'west':
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
	