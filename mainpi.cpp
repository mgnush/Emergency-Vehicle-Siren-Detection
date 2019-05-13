#include <cstdio>
#include <cstdlib>
#include <fftw3.h>
#include <math.h>
#include <bcm2835.h>
#include <sched.h>
#include <sys/mman.h>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <string>
#include <list>
#include <array>
#include "display.h"

// Extreme doppler effect coefficients
const double DOPPLER_MIN = 0.9592;
const double DOPPLER_MAX = 1.1777;
// Define frequency band of interest
const double BAND_FREQ_MIN  =700;
const double BAND_FREQ_MAX = 1600;
// Frequencies for multithresholding
const double NOISE_LOWMIN = 250;
const double NOISE_LOWMAX = 500;
const double NOISE_HIGHMIN = 1885;
const double NOISE_HIGHMAX = 3000;
// Number of bands for multithresholding
const int BANDS = 6;
const double NOISE_COEFF[BANDS] = {3.2,3.0,3.2,2.8,2.8,3.2};  
// Sampling constants (might need to check in program)
const double st = 2.058;   // Sampling time
const double fs = 8000;   // 8kHz sampling
const int N = 16464;   // # of samples
const int N_CH = 4;   // # of Mics
const char CHANNELS[4] = {0x80,0x90,0xa0,0xb0};   // Code to send to ADC
const int S = 2;   // # of fft_analysis to store (per channel)
// Testing-tuned microsecond delay to achieve 8kHz sampling
const int SAMPLE_DELAY = 55; //55; //21;  //87;  //90; //
// FFT-variables
const bool DOPPLER = true;
// Direction, location constants
const double DIR_MARGIN = 0.02;
const double LOC_MARGIN = 0.1;  // Used to compare two opposite sides if wall echo is suspected 

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
	fftw_plan p;
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
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_256); // 250MHz / 256 = ~1000kHz
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
	vars.out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (N / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays 
	vars.p = fftw_plan_dft_r2c_1d(N, vars.window, vars.out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
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
fft_analysis DoFFT(fft_vars &vars, const double *samples, const multi_thresh_indeces &mtIndeces, const bool &split, const int &i)
{
	fft_analysis fftAnal;
	int n = N;
	
	// Fill plan input array

	for (int j = 0; j < n; j++) {
		vars.window[j] = samples[n*i+j];
	}
	
	fftw_execute(vars.p); // Repeatable
	
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
	fftAnal.noiseThresh = totalNoise / ((mtIndeces.noiseIndexLowMax - mtIndeces.noiseIndexLowMin) + 
		(mtIndeces.noiseIndexHighMax - mtIndeces.noiseIndexHighMin));

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
int Detect(const fft_analysis &fftAnal, int (&detectedBands)[BANDS]) 
{
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

// Re-evaluate siren presence by merging consecutive half-windows when inconclusive
// number of bands are detected
void SplitWindowDetection(multi_thresh_indeces mtIndeces, int &detections, double *in[S][N_CH], double *inRev, fft_analysis &fftAnal, fft_vars &fftV, int &s, int &ch) 
{
	int detectionsRev = 0;
	int detectedBandsRev[BANDS];
	fft_analysis fftAnalRev;
	
	// Create new window consisting of latter half of previous window 
	// and first half of current window
	for (int j = N/2; j < N; j++) {
		inRev[j-(N/2)] = in[!s][ch][j];
	}
	for (int j = 0; j < N/2; j++) {
		inRev[j+(N/2)] = in[!s][ch][j];
	}
	
	// Analyse new window and replace results if better detection
	fftAnalRev = DoFFT(fftV, inRev, mtIndeces, false, 0);
	detectionsRev = Detect(fftAnalRev, detectedBandsRev);
	if (detectionsRev > detections) {
		detections = detectionsRev;
		fftAnal = fftAnalRev; 
	}
}

/* Runs the direction analysis, comparing two consecutive windows
 * \param[in] fftAnalPrev The FFT-analysis for the first window
 * \param[in] fftAnalCur The FFT-analysis for the second window
 * \param[in] detectedBands[2][BANDS] The two detection results
 */
direction Direction(const std::array<std::list<fft_analysis>, N_CH> fftAnals, location loc) 
{
	double relAvg = 0;
	
	int s = 0;
	double windowAvgs[S] = { 0 };   // l <= S
	for (fft_analysis l : fftAnals[(int)loc % N_CH]) {
		for (int j = 0; j < BANDS; j++) {
			windowAvgs[s] += l.bandAvgs[j] * (l.bandAvgs[j] >= NOISE_COEFF[j]);
		}
		windowAvgs[s] = windowAvgs[s] / BANDS;
		s++;
	}
	
	for (int s = 1; s < fftAnals[0].size(); s++) {
		relAvg += windowAvgs[s] / windowAvgs[s - 1];
	}
	
	if (relAvg > (1 + DIR_MARGIN)) {
		printf("Detected EV is approaching at %f. \n", relAvg);
		return approaching;
	}
	else if (relAvg < (1 - DIR_MARGIN)) {
		printf("Detected EV is moving away at %f. \n", relAvg);
		return receding;
	}
	else {
		printf("Detected direction is inconclusive at %f. \n", relAvg);
		return no_dir;
	}
} 

location Location(std::array<std::list<fft_analysis>, N_CH> fftAnals, const int (&detectedBands)[N_CH][2][BANDS]) 
{
	double windowAvgs[N_CH] = { 0 };
	for (int ch = 0; ch < N_CH; ch++) {
		for (int j = 0; j < BANDS; j++) {
			windowAvgs[ch] += (fftAnals[ch].back().bandAvgs[j]); 
		}
		windowAvgs[ch] = windowAvgs[ch] / BANDS;
	}
	
	location loc = (location)0;

	double maxAvg = windowAvgs[0];
	for (int ch = 1; ch < N_CH; ch++) {
		if (windowAvgs[ch] > maxAvg) {
			maxAvg = windowAvgs[ch];
			loc = (location)ch;
		} 		
	}
	
	// If the indicated location and the opposite side are within LOC_MARGIN % of each other,
	// a wall might be present. Conclude that location can't be determined confidently
	if (maxAvg < (1+LOC_MARGIN) * windowAvgs[((int)loc + 2) % N_CH]) {
		loc = no_loc;
	}
		
		
	printf("The EV was detected in direction %d. \n", loc);
	
	return loc;
}

int main() 
{
	SpiSetup();
	initialize_display_pins();
	
	multi_thresh_indeces mtIndeces = SetupMultiThresholding(N, DOPPLER);

	fft_vars fftV = SetupFFT();
	
	double timeSpan; // Actual sampling time
	bool firstRunFlag = true; // Prevent 'empty' array from being used
	int evPresent = 0;
	double *in[2][N_CH]; // Store 2 consecutive sampling windows at a time for each channel
	double *inRev;
	for (int s = 0; s < 2; s++) {
		for (int ch = 0; ch < N_CH; ch++) {
			in[s][ch] = (double*)malloc(sizeof(double) * N);
			in[s][ch] = (double*)malloc(sizeof(double) * N);
		}
	}
	inRev = (double*)malloc(sizeof(double) * N);
	std::array<std::list<fft_analysis>, N_CH> fftAnals;
	//fft_analysis fftAnal[S][N_CH];
	int detectedBands[N_CH][2][BANDS];
	int detections[N_CH];
	location loc;
	direction dir = no_dir;
	int cycles = MAX_CYCLES + 1; // # of sampling windows since last detection. init to prevent dir being run on first det
	//std::string exit;
	
	while (1) {
		
		for (int s = 0; s < 2; s++) {
			timeSpan = DoSampling(in[s]);		
			printf("The sampling window of %d samples was %f seconds \n", N, timeSpan);
			
			auto begin = std::chrono::high_resolution_clock::now();   // Testing only
			
			// Detection on each channel
			for (int ch = 0; ch < N_CH; ch++) {
				printf("Channel %d: ", ch); 
				
				fftAnals[ch].push_back(DoFFT(fftV, in[s][ch], mtIndeces, false, 0));
				detections[ch] = Detect(fftAnals[ch].back(), detectedBands[ch][s]);
				
				// TESTING ONLY
				//FftPrint("mcp3008test.txt", fftV.absFFT, (double)fs / (double)n);
				
				if (((detections[ch] > 0) && (detections[ch] <= (BANDS / 2))) && (!firstRunFlag)) { 
					SplitWindowDetection(mtIndeces, detections[ch], in, inRev, fftAnals[ch].back(), fftV, s, ch); 
				} 
				
				printf("Siren was detected in %d out of %d bands \n", detections[ch], BANDS);
				
				evPresent += (detections[ch] > (BANDS / 2) );   // Detection verdict - only one channel needs to detect
				
				if (fftAnals[ch].size() > S) { fftAnals[ch].pop_front(); }	// Maintain list to specified length
				
				firstRunFlag = false;
			}
			
			// Direction, Location, UI
			if (evPresent) {				
				loc = Location(fftAnals, detectedBands);
				if (cycles == 0) { dir = Direction(fftAnals, loc); } // Only run direction on two consecutive detections
				cycles = 0;   // 0 windows since last detection
				evPresent = 0;
			} else {
				cycles++;
				dir = no_dir;
			}		
				
			update_display(cycles, loc, dir);
						
			auto end = std::chrono::high_resolution_clock::now();
			double tim = std::chrono::duration_cast<std::chrono::milliseconds>(end-begin).count();
			printf("Algorithms took %.1fms \n \n", tim);
		}
	}
	
	// Free resources
	for (int s = 0; s < 2; s++) {
		for (int ch = 0; ch < N_CH; ch++) {
			free(in[s][ch]);
		}
	}
	free(inRev);
	fftw_destroy_plan(fftV.p);
	fftw_free(fftV.out);
	free(fftV.absFFT);
	
	bcm2835_spi_end();
	bcm2835_close();
	
	printf("Program ended \n");
		
	return 0;
}
