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
#include <string>
#include "display.h"

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
const int N_CH = 3;   // # of Mics
const char CHANNELS[4] = {0x80,0x90,0xa0,0xb0};   // Code to send to ADC
// Testing-tuned microsecond delay to achieve 8kHz sampling
const int SAMPLE_DELAY = 21; //55; //21;  //87;  //90; //
// FFT-variables
const int SPLIT = 4; // Amount of subwindows per parent window
const bool DOPPLER_PARENT = true;
const bool DOPPLER_SUB = false;
// Direction, location constants
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

void FftPrint(const char* fileName, double *data, const double df)
{
	std::ofstream myFile;
	myFile.open(fileName);

	for (int i = 0; i < N/2-1; i++) {
		myFile << (double)i*df << ": " << data[i]*(N/2) << "\n";
	}
	myFile.close();
}

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
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_512); // 250MHz / 256 = ~1000kHz
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
}

/* Creates a multi_thresh_indeces variable containing the array
 * indeces representing relevant band frequcneis to be used in multithresholding
 * \param[in] n The window length to setup for
 * \param[in] doppler Whether doppler's effect will be accounted for
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

/* Performs an entire sample window (all channels), saving the results in the
 * parameter array. 
 * \param[in] *samples[N_CH] The array that will hold all samples for this window
 * \return The time taken to complete the sampling window
*/
double DoSampling(double *samples[N_CH])
{
	static char mosi[4][3] = {{0x01,CHANNELS[0],0x00},{0x01,CHANNELS[1],0x00},{0x01,CHANNELS[2],0x00},{0x01,CHANNELS[3],0x00}};
	char miso[3] = { 0 };
	
	// Set process as highest priority in the OS scheduler
	struct sched_param sp;
	//memset(&sp, 0, sizeof(sp));
	sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	sched_setscheduler(0, SCHED_FIFO, &sp);
	
	if (mlockall(MCL_CURRENT | MCL_FUTURE) == 0) {   // Prevent paging
		auto begin = std::chrono::high_resolution_clock::now();
		for(int i = 0; i < N; i++) {
			for (int j = 0; j < N_CH; j++) {
				bcm2835_spi_transfernb(mosi[j], miso, 3); // send/receive 3 bytes
				samples[j][i] = (miso[1] << 8) + miso[2];
			}
			bcm2835_delayMicroseconds(SAMPLE_DELAY);
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
 * \param[in] vars The fft variables
 * \param[in] *samples The samples to be FFT'd
 * \param[in] mtIndeces The multithresholding variables
 * \param[in] split Whether the window to analyse is a subwindow
 * \param[in] i The subwindow to FFT - 0 if parent window
 * \return The FFT-analysis in the form of multithresholding average band values
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
		fftAnal.bandAvgs[j] = totalVol[j] / mtIndeces.bandLength / fftAnal.noiseThresh;
	}

	// Print for testing
	printf("Window %d: The noise threshold is %f, and the band averages are", i, fftAnal.noiseThresh);
	for (int j = 0; j < BANDS; j++) {
		printf(" %.2f ", fftAnal.bandAvgs[j]);
	}
	printf("\n");

	return fftAnal;
}

/* Runs the detection algorithm on the parameter fft-analysis
 * \param[in] fftAnal The FFT-analysis
 * \param[in] detectedBands[BANDS] The array to hold the detection results
 * \return The number of bands that detected an EV.
 */
int Detect(const fft_analysis &fftAnal, int (&detectedBands)[BANDS]) {
	int detections = 0;

	for (int i = 0; i < BANDS; i++) {
		detectedBands[i] = (fftAnal.bandAvgs[i] >= NOISE_COEFF[i]);
		detections += detectedBands[i];
	}

	//printf("The siren is present in %d out of %d bands, and they are:", detections, BANDS);

	for (int j = 0; j < BANDS; j++)	{
		printf(" %d ", detectedBands[j]);
	}
	printf("\n");
	
	return detections;
}

/* Runs the direction analysis, comparing two consecutive parent windows
 * \param[in] fftAnalPrev The FFT-analysis for the first window
 * \param[in] fftAnalCur The FFT-analysis for the second window
 * \param[in] detectedBands[2][BANDS] The two detection results
 */
direction DirectionParentOnly(const fft_analysis &fftAnalPrev, fft_analysis &fftAnalCur, const int (&detectedBands)[2][BANDS]) {
	double windowAvgs[2] = { 0 };
	fft_analysis fftAnals[2] = {fftAnalPrev, fftAnalCur};

	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < BANDS; j++) {
			windowAvgs[i] += (fftAnals[i].bandAvgs[j]); // * detectedBands[i][j]);
		}
		windowAvgs[i] = windowAvgs[i] / BANDS;
	}

	for (int i = 1; i < 2; i++) {
		double rel = windowAvgs[i] / windowAvgs[i - 1];
		if (rel > (1 + DIR_MARGIN)) {
			printf("Detected EV is approaching at %f. \n", rel);
			return approaching;
		}
		else if (rel < (1 - DIR_MARGIN)) {
			printf("Detected EV is moving away at %f. \n", rel);
			return receding;
		}
		else {
			printf("Detected direction is inconclusive at %f. \n", rel);
			return no_dir;
		}
	}
}

location Location(const fft_analysis (&fftAnals)[N_CH], const int (&detectedBands)[N_CH][2][BANDS], const int &i) 
{
	double windowAvgs[N_CH] = { 0 };
	for (int ch = 0; ch < N_CH; ch++) {
		for (int j = 0; j < BANDS; j++) {
			windowAvgs[ch] += (fftAnals[ch].bandAvgs[j] * detectedBands[ch][i][j]);
		}
		windowAvgs[ch] = windowAvgs[ch] / BANDS;
	}
	
	location loc = (location)0;
	for (int ch = 1; ch < N_CH; ch++) {
		double rel = windowAvgs[ch] / windowAvgs[ch - 1];
		if (rel > (1 + 0)) {
			loc = (location)ch;
		} 
	}
	printf("The EV was detected in direction %d. \n", loc);
	
	return loc;
}

int main() 
{
	SpiSetup();
	initialize_display_pins();
	
	multi_thresh_indeces mtIndeces = SetupMultiThresholding(N, DOPPLER_PARENT);
	multi_thresh_indeces mtIndecesSub = SetupMultiThresholding(N/SPLIT, DOPPLER_SUB);

	fft_vars fftV = SetupFFT();
	
	double timeSpan; // Actual sampling time
	bool firstRunFlag = true; // Prevent 'empty' array from being used
	bool evPresent = false;
	double *in[2][N_CH]; // Store two sampling windows at a time for each channel
	double *inRev[N_CH];
	for (int ch = 0; ch < N_CH; ch++) {
		in[0][ch] = (double*)malloc(sizeof(double) * N);
		in[1][ch] = (double*)malloc(sizeof(double) * N);
		inRev[ch] = (double*)malloc(sizeof(double) * N);
	}
	fft_analysis fftAnal[2][N_CH], fftAnalRev[N_CH];
	int detectedBands[N_CH][2][BANDS];
	int detections[N_CH], detectionsRev[N_CH];
	location loc;
	direction dir[N_CH];
	int cycles = 0; // # of sampling windows since last detection
	//std::string exit;
	
	while (1) {
		
		for (int i = 0; i < 2; i++) {
			timeSpan = DoSampling(in[i]);
			
			auto begin = std::chrono::high_resolution_clock::now();
			
			printf("The sampling window of %d samples was %f seconds \n", N, timeSpan);
			for (int ch = 0; ch < N_CH; ch++) {
				printf("Channel %d: ", ch); 
				
				fftAnal[i][ch] = DoFFT(fftV, in[i][ch], mtIndeces, false, 0);
				detections[ch] = Detect(fftAnal[i][ch], detectedBands[ch][i]);
				
				// TESTING ONLY
				//FftPrint("mcp3008test.txt", fftV.absFFT, (double)fs / (double)n);
				
				// Re-evaluate siren presence by merging consecutive half-windows when inconclusive
				// number of bands are detected
				if (((detections[ch] > 0) && (detections[ch] <= (BANDS / 2))) && (!firstRunFlag)) {
					for (int j = N/2; j < N; j++) {
						inRev[ch][j-(N/2)] = in[!i][ch][j];
					}
					for (int j = 0; j < N/2; j++) {
						inRev[ch][j+(N/2)] = in[!i][ch][j];
					}
					fftAnalRev[ch] = DoFFT(fftV, inRev[ch], mtIndeces, false, 0);
					detectionsRev[ch] = Detect(fftAnalRev[ch], detectedBands[ch][i]);
					if (detectionsRev[ch] > detections[ch]) {
						detections[ch] = detectionsRev[ch];
						fftAnal[i][ch] = fftAnalRev[ch]; // Test
					}
				}
				
				printf("Siren was detected in %d out of %d bands \n", detections[ch], BANDS);
				
				// Direction
				if (detections[ch] >= ((BANDS / 2) + 1)) {
					dir[ch] = DirectionParentOnly(fftAnal[i][ch], fftAnal[!i][ch], detectedBands[ch]);
					evPresent = true;
				}
				firstRunFlag = false; 
			}
			
			// Location & UI
			if (evPresent) {
				loc = Location(fftAnal[i], detectedBands, i);
				cycles = 0;
				evPresent = false;
			} else {
				cycles++;
			}			
			
			update_display(cycles, loc, dir[loc]);
			
			auto end = std::chrono::high_resolution_clock::now();
			double tim = std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count();
			printf("Algorithms took %.1fms \n \n", tim);
		}
	}
	
	// Free resources
	for (int i = 0; i < N_CH; i++) {
		free(in[0][i]); free(in[1][i]); free(inRev[i]);
	}
	fftw_destroy_plan(fftV.p[0]);
	fftw_destroy_plan(fftV.p[1]);
	fftw_free(fftV.out);
	free(fftV.absFFT);
	
	bcm2835_spi_end();
	bcm2835_close();
	
	printf("Program ended \n");
		
	return 0;
}
