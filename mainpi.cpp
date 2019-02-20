#include <cstdio>
#include <cstdlib>
#include <fftw3.h>
#include <math.h>
#include <bcm2835.h>
#include <sched.h>
#include <sys/mman.h>
#include <chrono>
#include <iostream>
#include <fstream>

// Extreme doppler effect coefficients
const double DOPPLER_MIN = 0.8491;
const double DOPPLER_MAX = 1.0425;
// Define doppler-adjusted frequency band of interest
const double threshLow = 700 * DOPPLER_MAX;
const double threshHigh = 1550 * DOPPLER_MIN;
// Frequencies for multithresholding
const double noiseThreshLowMin = 200;
const double noiseThreshLowMax = 500;
const double noiseThreshHighMin = 1885;
const double noiseThreshHighMax = 3000;
// Number of bands for multithresholding
const int BANDS = 6;
const double noiseMultiplier[BANDS] = {2.6,2.5,2.6,2.5,2.7,2.5}; //{2.6,2.5,2.3,2.4}; 
// Sampling constants (might need to check in program)
const double st = 2.058;   // Sampling time
const double fs = 8000;   // 8kHz sampling
const int n = 16464;   // # of samples
const double df = fs / n;
// Multithresholding constants
const int noiseIndexLowMin = (int)(noiseThreshLowMin / df);
const int noiseIndexLowMax = (int)(noiseThreshLowMax / df);
const int noiseIndexHighMin = (int)(noiseThreshHighMin / df);
const int noiseIndexHighMax = (int)(noiseThreshHighMax / df);
// Testing-tuned microsecond delay to achieve 8kHz sampling
const int sampleDelay = 79;

/* Configures the SPI communication with the ADC 
*/
void spiSetup() 
{
	if (!bcm2835_init())
	{
		printf("bcm2835 initialisation failed \n");
		exit(1);
	}
	
	bcm2835_spi_begin();
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0); // Data comes in on falling edge
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_1024); // 250MHz / 1024 = ~250kHz
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
}

/* Populates the parameter array with the fft indeces corrsponding
 * to the frequencies that define and separate the bands in the
 * multithresholding scheme, from low to high frequencies.
 * \param[in] bandIndeces[BANDS+1} The array of indeces to be populated
*/
int multiThresholdSetup(int (&bandIndeces)[BANDS+1])
{
	// Find array indeces
	bandIndeces[0] = (int)(threshLow / df);
	bandIndeces[BANDS] = (int)(threshHigh / df);
	int bandLength = (bandIndeces[BANDS] - bandIndeces[0]) / BANDS;
	
	for (int i = 1; i < BANDS; i++) {
		bandIndeces[i] = bandIndeces[i - 1] + bandLength;
	}
	
	return bandLength;
}

/* Performs an entire sample window, saving the results in the
 * parameter array of integers. 
 * \param[in] samples[n] The array that will hold all samples
 * \return The time taken to complete the sampling window
*/
double doSampling(double *samples)
{
	char mosi[2] = {0x68,0x00}; // 01101000, x -> 0,1,single=1,cs,msbf
	char miso[2] = { 0 };
	int i = 0;
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {   // Prevent paging
		auto begin = std::chrono::high_resolution_clock::now();
		while (i < n) {
			bcm2835_spi_transfernb(mosi, miso, 2); // send/receive 2 bytes
			samples[i] = (miso[0] << 8) + miso[1];
			bcm2835_delayMicroseconds(sampleDelay);
			i++;
		}	
		auto end = std::chrono::high_resolution_clock::now();
		munlockall();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count() / 1000.0; // Get actual time
	} else exit(1);
}

/* Employs the multi-thresholding scheme to determine in
 * which of the bands the siren appears to be present.
 * \param[in] *ffts The complex fft to be analysed
 * \param[in] bandIndeces[BANDS+1] The fft indeces representing the 
 * 			  border of the bands
 * \param[in] bandLength The width (in indeces) of the bands
 * \return The number of bands in which the siren is present
*/
int analyseFFT(const fftw_complex *ffts, const int (&bandIndeces)[BANDS+1], const int &bandLength, double *absFFT)
{
	int detection[BANDS] = { 0 };   // Multithreshold detection results
	// Obtain absolute FFT
	
	absFFT[0] = ffts[0][0]/n;
	for (int i = 1; i < ((n / 2) - 1); i++) {
		absFFT[i] = 2*sqrt(pow(ffts[i][0], 2) + pow(ffts[i][1],2))/n;
	}
	// Obtain noise levels
	double totalNoise = 0;
	for (int i = noiseIndexLowMin; i < noiseIndexLowMax; i++) {
		totalNoise += absFFT[i];
	}
	for (int i = noiseIndexHighMin; i < noiseIndexHighMax; i++) {
		totalNoise += absFFT[i];
	}
	double noiseThresh = totalNoise / ((noiseIndexLowMax - noiseIndexLowMin) + (noiseIndexHighMax - noiseIndexHighMin));
	// Obtain BoI (Bands of Interest) levels
	double totalVol[BANDS] = { 0 };
	double avgVol[BANDS] = { 0 };
	int detections = 0;
	for (int i = 0; i < BANDS; i++) {
		for (int j = bandIndeces[i]; j < bandIndeces[i + 1]; j++) {
			totalVol[i] += absFFT[j];
		}
		avgVol[i] = totalVol[i] / bandLength;
		detection[i] = (avgVol[i] >= (noiseThresh*noiseMultiplier[i]));
		detections += detection[i];
	}
	
	//TESTING ONLY
	for (int i = 0; i < BANDS; i++) {
		printf(" %.2f ", avgVol[i]/noiseThresh-noiseMultiplier[i]);
	}
	printf("\n");
	
	return detections;
}

int main() 
{
	spiSetup();
	
	int bandIndeces[BANDS+1] = { 0 };   // Band limit values (fft index)
	int bandLength = multiThresholdSetup(bandIndeces);
		
	// Set up FFT
	double *in[2], *in_rev; 
	fftw_complex *out;   // Complex 1D, n/2-1 length output
	fftw_plan p[2], p_rev;
	in[0] = (double*) malloc(sizeof(double) * 2 * (n / 2 - 1));
	in[1]= (double*) malloc(sizeof(double) * 2 * (n / 2 - 1));
	in_rev = (double*) malloc(sizeof(double) * 2 * (n / 2 - 1));
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (n / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays
	p[0] = fftw_plan_dft_r2c_1d(n, in[0], out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
	p[1] = fftw_plan_dft_r2c_1d(n, in[1], out, FFTW_ESTIMATE);
	p_rev = fftw_plan_dft_r2c_1d(n, in_rev, out, FFTW_ESTIMATE);
	double *absFFT = (double*)malloc(sizeof(double) * (n / 2 - 1));
	
	double timeSpan = 0.0; // Actual sampling time
	int firstRunFlag = 1; // Prevent 'empty' array from being used
	int sirenPresent, sirenPresent_rev;
	
	while (1) {
		// Store two sampling windows at a time
		for (int i = 0; i < 2; i++) {
			timeSpan = doSampling(in[i]);
			printf("The sampling window of %d samples was %f seconds \n", n, timeSpan);
			
			fftw_execute(p[i]);
			
			sirenPresent = analyseFFT(out, bandIndeces, bandLength, absFFT);
			
			// Re-evaluate siren presence by merging consecutive windows when 1 or 2 bands 
			// are detected
			if (((sirenPresent > 0) && (sirenPresent < 3)) && (!firstRunFlag)) {
				for (int j = n/2; j < n; j++) {
					in_rev[j-(n/2)] = in[!i][j];
				}
				for (int j = 0; j < n/2; j++) {
					in_rev[j+(n/2)] = in[!i][j];
				}
				fftw_execute(p_rev);
				sirenPresent_rev = analyseFFT(out, bandIndeces, bandLength, absFFT);
				if (sirenPresent_rev > sirenPresent) {
					sirenPresent = sirenPresent_rev;
				}
			}
			
			printf("Siren was detected in %d out of %d bands \n", sirenPresent, BANDS);
			
			firstRunFlag = 0; 
		}
	}
	
	
	// Output FFT-results
	/*std::ofstream myFile;
	myFile.open("fftresults.txt");

	for (int i = 0; i < (n/2); i++) {
		myFile << out[i][0] << ", " << out[i][1] << "\n";
	}
	myFile.close();
	*/

	fftw_destroy_plan(p[0]); fftw_destroy_plan(p[1]); fftw_destroy_plan(p_rev);
	fftw_free(out); free(absFFT); //in is destroyed by plan execution
	
	bcm2835_spi_end();
	bcm2835_close();
	
	printf("Program ended \n");
		
	return 0;
}
