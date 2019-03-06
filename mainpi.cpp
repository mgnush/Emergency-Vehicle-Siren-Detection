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
// Define frequency band of interest
const double BAND_FREQ_MIN  =700;
const double BAND_FREQ_MAX = 1550;
// Frequencies for multithresholding
const double NOISE_LOWMIN = 200;
const double NOISE_LOWMAX = 500;
const double NOISE_HIGHMIN = 1885;
const double NOISE_HIGHMAX = 3000;
// Number of bands for multithresholding
const int BANDS = 6;
const double NOISE_COEFF[BANDS] = {2.6,2.5,2.8,2.9,2.9,2.8};   //{2.6,2.5,2.6,2.5,2.7,2.5}; //{2.6,2.5,2.3,2.4}; 
// Sampling constants (might need to check in program)
const double st = 2.058;   // Sampling time
const double fs = 8000;   // 8kHz sampling
const int N = 16464;   // # of samples
// Testing-tuned microsecond delay to achieve 8kHz sampling
const int SAMPLE_DELAY = 79;
// FFT-variables
const int SPLIT = 4; // Amount of subwindows per parent window
const bool DOPPLER_PARENT = true;
const bool DOPPLER_SUB = false;
// Direction variables
const double DIR_MARGIN = 0.02;

struct multi_thresh_indeces {
	int bandIndeces[BANDS + 1];
	int bandLength;
	int noiseIndexLowMin;
	int noiseIndexLowMax;
	int noiseIndexHighMin;
	int noiseIndexHighMax;
};

struct fft_vars {
	double *window;
	fftw_complex *out;   
	fftw_plan p[2];
	double *absFFT;
};

struct fft_analysis {
	double bandAvgs[BANDS];
	double noiseThresh;
};

/* Configures the SPI communication with the ADC 
*/
void SpiSetup() 
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

/* Creates a multi_thresh_indeces variable containing the array
 * indeces representing relevant band frequcneis to be used in multithresholding
 * \param[in] n the window length to setup for
*/
multi_thresh_indeces SetupMultiThresholding(const int &n, const bool &doppler)
{
	multi_thresh_indeces mtIndeces;

	double threshLow = BAND_FREQ_MIN * (1 + (doppler * (DOPPLER_MAX - 1)));
	double threshHigh = BAND_FREQ_MAX * (1 + (doppler * (DOPPLER_MIN - 1)));
	// TESTING ONLY
	printf("The band minimum is %.1f and the maximum is %.1f \n", threshLow, threshHigh);
	
	// Find array indeces
	double df = (double)fs / (double)n;
	mtIndeces.bandIndeces[0] = (int)(threshLow / df);
	mtIndeces.bandIndeces[BANDS] = (int)(threshHigh / df);
	mtIndeces.bandLength = (mtIndeces.bandIndeces[BANDS] - mtIndeces.bandIndeces[0]) / BANDS;
	for (int i = 1; i < BANDS; i++) {
		mtIndeces.bandIndeces[i] = mtIndeces.bandIndeces[i - 1] + mtIndeces.bandLength;
	}

	mtIndeces.noiseIndexLowMin = (int)(NOISE_LOWMIN / df);
	mtIndeces.noiseIndexLowMax = (int)(NOISE_LOWMAX / df);
	mtIndeces.noiseIndexHighMin = (int)(NOISE_HIGHMIN / df);
	mtIndeces.noiseIndexHighMax = (int)(NOISE_HIGHMAX / df);

	return mtIndeces;
}

/* Performs an entire sample window, saving the results in the
 * parameter array of integers. 
 * \param[in] samples[n] The array that will hold all samples
 * \return The time taken to complete the sampling window
*/
double DoSampling(double *samples)
{
	char mosi[2] = {0x68,0x00}; // 01101000, x -> 0,1,single=1,cs,msbf
	char miso[2] = { 0 };
	int i = 0;
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {   // Prevent paging
		auto begin = std::chrono::high_resolution_clock::now();
		while (i < N) {
			bcm2835_spi_transfernb(mosi, miso, 2); // send/receive 2 bytes
			samples[i] = (miso[0] << 8) + miso[1];
			bcm2835_delayMicroseconds(SAMPLE_DELAY);
			i++;
		}	
		auto end = std::chrono::high_resolution_clock::now();
		munlockall();
		return std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count() / 1000.0; // Get actual time
	} else exit(1);
}

/* Creates and allocates the variables needed to perform FFT repeatedly
*/ 
fft_vars SetupFFT() {
	fft_vars vars;
	
	vars.window = (double*)malloc(sizeof(double) * 2 * (N / 2 - 1));
	vars.out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (N / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays   // Complex 1D, n/2-1 length output
	vars.p[0] = fftw_plan_dft_r2c_1d(N, vars.window, vars.out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
	vars.p[1] = fftw_plan_dft_r2c_1d(N/SPLIT, vars.window, vars.out, FFTW_ESTIMATE); // This plan is for analysing subwindows. 
	vars.absFFT = (double*)malloc(sizeof(double) * (N / 2 - 1));	

	return vars;
}

/* Employs the multi-thresholding scheme and returns an array
 * of average band volumes.
 * \param[in] *ffts The complex fft to be analysed
 * \param[in] bandIndeces[BANDS+1] The fft indeces representing the 
 * 			  border of the bands
 * \param[in] bandLength The width (in indeces) of the bands
 * \return The number of bands in which the siren is present
*/
fft_analysis DoFFT(fft_vars vars, const double *samples, const multi_thresh_indeces &mtIndeces, const bool &split, const int &i)
{
	fft_analysis fftAnal;
	int n = N;
	
	if (split) {
		n /= SPLIT;
	}
	
	// Fill plan input array

	for (int j = 0; j < n; j++) {
		vars.window[j] = samples[n*i+j];
	}

		
	fftw_execute(vars.p[split]); // Repeatable. Run the plan parent/sub plan depending on split param

	// Obtain absolute, normalised FFT
	vars.absFFT[0] = vars.out[0][0] / n;
	for (int j = 1; j < (n/ 2 - 1); j++) {
		vars.absFFT[j] = 2 * sqrt(pow(vars.out[j][0], 2) + pow(vars.out[j][1], 2)) / n;
	}

	// Obtain noise levels
	double totalNoise = 0;
	for (int j = mtIndeces.noiseIndexLowMin; j < mtIndeces.noiseIndexLowMax; j++) {
		totalNoise += vars.absFFT[j];
	}
	for (int j = mtIndeces.noiseIndexHighMin; j < mtIndeces.noiseIndexHighMax; j++) {
		totalNoise += vars.absFFT[j];
	}
	fftAnal.noiseThresh = totalNoise / ((mtIndeces.noiseIndexLowMax - mtIndeces.noiseIndexLowMin) + (mtIndeces.noiseIndexHighMax - mtIndeces.noiseIndexHighMin));

	// Obtain BoI (Bands of Interest) levels
	double totalVol[BANDS] = { 0 };

	for (int j = 0; j < BANDS; j++) {
		for (int k = mtIndeces.bandIndeces[j]; k < mtIndeces.bandIndeces[j + 1]; k++) {
			totalVol[j] += vars.absFFT[k];
		}
		fftAnal.bandAvgs[j] = totalVol[j] / mtIndeces.bandLength;
	}

	// Print for testing
	printf("Window %d: The noise threshold is %f, and the band averages are", i, fftAnal.noiseThresh);
	for (int j = 0; j < BANDS; j++) {
		printf(" %f ", fftAnal.bandAvgs[j]/fftAnal.noiseThresh);
	}
	printf("\n");

	return fftAnal;
}

int Detect(const fft_analysis &fftAnal, int(&detectedBands)[BANDS]) {
	int detections = 0;

	for (int i = 0; i < BANDS; i++) {
		detectedBands[i] = (fftAnal.bandAvgs[i] >= (fftAnal.noiseThresh * NOISE_COEFF[i]));
		detections += detectedBands[i];
	}

	printf("The siren is present in %d out of %d bands, and they are:", detections, BANDS);

	for (int j = 0; j < BANDS; j++)	{
		printf(" %d ", detectedBands[j]);
	}
	printf("\n");
	
	return detections;
}

// Compare only parent windows
void DirectionParentOnly(const fft_analysis fftAnalPrev, fft_analysis fftAnalCur, const int (&detectedBands)[2][BANDS]) {
	double windowAvgs[2] = { 0 };
	fft_analysis fftAnals[2] = {fftAnalPrev, fftAnalCur};

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < BANDS; j++) {
			windowAvgs[i] += (fftAnals[i].bandAvgs[j] * detectedBands[i][j]);
		}
		windowAvgs[i] = windowAvgs[i] / BANDS;
	}

	for (int i = 1; i < 2; i++) {
		double rel = windowAvgs[i] / windowAvgs[i - 1];
		if (rel > (1 + DIR_MARGIN)) {
			printf("Detected EV is approaching at %f. \n", rel);
		}
		else if (rel < (1 - DIR_MARGIN)) {
			printf("Detected EV is moving away at %f. \n", rel);
		}
		else {
			printf("Detected direction is inconclusive at %f. \n", rel);
		}
	}
}

int main() 
{
	SpiSetup();
	
	multi_thresh_indeces mtIndeces = SetupMultiThresholding(N, DOPPLER_PARENT);
	multi_thresh_indeces mtIndecesSub = SetupMultiThresholding(N/SPLIT, DOPPLER_SUB);

	fft_vars fftV = SetupFFT();
	
	double timeSpan; // Actual sampling time
	bool firstRunFlag = true; // Prevent 'empty' array from being used
	double *in[2]; // Store two sampling windows at a time
	double *inRev;
	in[0] = (double*)malloc(sizeof(double) * N);
	in[1] = (double*)malloc(sizeof(double) * N);
	inRev = (double*)malloc(sizeof(double) * N);
	fft_analysis fftAnal[2], fftAnalRev;
	int detectedBands[2][BANDS];
	int detections, detectionsRev;
	
	while (1) {
		
		for (int i = 0; i < 2; i++) {
			timeSpan = DoSampling(in[i]);
			printf("The sampling window of %d samples was %f seconds \n", N, timeSpan);
			
			fftAnal[i] = DoFFT(fftV, in[i], mtIndeces, false, 0);
			detections = Detect(fftAnal[i], detectedBands[i]);
			
			// Re-evaluate siren presence by merging consecutive windows when 1 or 2 bands 
			// are detected
			if (((detections > 0) && (detections <= (BANDS / 2))) && (!firstRunFlag)) {
				for (int j = N/2; j < N; j++) {
					inRev[j-(N/2)] = in[!i][j];
				}
				for (int j = 0; j < N/2; j++) {
					inRev[j+(N/2)] = in[!i][j];
				}
				fftAnalRev = DoFFT(fftV, inRev, mtIndeces, false, 0);
				detectionsRev = Detect(fftAnalRev, detectedBands[i]);
				if (detectionsRev > detections) {
					detections = detectionsRev;
				}
			}
			
			printf("Siren was detected in %d out of %d bands \n", detections, BANDS);
			
			// Detection
			if (detections >= ((BANDS / 2) + 1)) {
				DirectionParentOnly(fftAnal[i], fftAnal[!i], detectedBands);
			}
			
			firstRunFlag = false; 
		}
	}
	
	free(in[0]); free(in[1]); free(inRev);
	fftw_destroy_plan(fftV.p[0]);
	fftw_destroy_plan(fftV.p[1]);
	fftw_free(fftV.out);
	free(fftV.absFFT);
	
	bcm2835_spi_end();
	bcm2835_close();
	
	printf("Program ended \n");
		
	return 0;
}
