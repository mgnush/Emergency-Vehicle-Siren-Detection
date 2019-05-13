// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
/*
1. Read sound file
2. Set up multithresholding
3. Perform FFT
4. Analyse FFT
*/

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sndfile.h>
#include <fftw3.h>
#include <iostream>
#include <fstream>
#include <math.h>

// Extreme doppler effect coefficients
const double DOPPLER_MIN = 0.8491;
const double DOPPLER_MAX = 1.0425;
// Define doppler-adjusted frequency band of interest
const double BAND_FREQ_MIN = 700;
const double BAND_FREQ_MAX = 1550;
// Frequencies for multithresholding
const double noiseThreshLowMin = 150;
const double noiseThreshLowMax = 510;
const double noiseThreshHighMin = 1885;
const double noiseThreshHighMax = 3000;
// Number of bands for multithresholding
const int BANDS = 6;
const int noiseMultiplier = 2.5;
// Sampling constants
const double fullWindow = 2.058; // Seconds
const int W = 2; // Number of windows to keep
// Directionality constants
const int SPLIT = 2;
const double subWindow = fullWindow / SPLIT;
const int SW = SPLIT * W;
const double dirMargin = 0.02; // Changes below this magnitude (%) are considered to be inconclusive

struct rec_data {
	int channels;
	long n;
	int fs;
	double *samples;
};

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

void fftToFile(const char* fileName, fftw_complex *&data)
{
	std::ofstream myFile;
	myFile.open(fileName);

	for (int i = 0; i < 5; i++) {
		myFile << data[i][0] << ", " << data[i][1] << "\n";
	}
	myFile.close();
}

rec_data readRecording(const char* fileName) {
	// Open soundfile
	SNDFILE *policeSiren;
	SF_INFO sfinfo;

	if (!(policeSiren = sf_open(fileName, SFM_READ, &sfinfo))) {
		printf("Not able to open sound file %s \n", fileName);
		puts(sf_strerror(NULL));
		exit(1);
	}

	// Soundfile variables
	rec_data data;
	data.channels = sfinfo.channels;
	data.n = sfinfo.frames;
	data.fs = sfinfo.samplerate;

	// Fetch sound data
	data.samples = (double*)malloc(sizeof(double) * data.channels * data.n);
	sf_count_t itemsExpected = data.channels * data.n;
	sf_count_t items = sf_read_double(policeSiren, data.samples, itemsExpected);

	if (itemsExpected != items) {
		printf("File data error \n");
		exit(1);
	}
	sf_close(policeSiren);

	printf("There are %d channels, %d frames, rate is %d. \n", data.channels, data.n, data.fs);

	return data;
}

multi_thresh_indeces setupMultiThresholding(const int &n, const int &fs, const bool &doppler) {

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

	mtIndeces.noiseIndexLowMin = (int)(noiseThreshLowMin / df);
	mtIndeces.noiseIndexLowMax = (int)(noiseThreshLowMax / df);
	mtIndeces.noiseIndexHighMin = (int)(noiseThreshHighMin / df);
	mtIndeces.noiseIndexHighMax = (int)(noiseThreshHighMax / df);

	return mtIndeces;
}

fft_vars setupFFT(const int &nWindow) {
	fft_vars vars;
	vars.window = (double*)malloc(sizeof(double) * 2 * (nWindow / 2 - 1));
	vars.out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (nWindow / 2 - 1));   // In-place fft requires in and 
																						   // out to accomodate two out-arrays   
																						   // Complex 1D, n/2-1 length output
	vars.p = fftw_plan_dft_r2c_1d(nWindow, vars.window, vars.out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial 
																				  // plan execution. ESTIMATE has no initial timecost.
	vars.absFFT = (double*)malloc(sizeof(double) * (nWindow / 2 - 1));	

	return vars;
}

fft_analysis doFFT(fft_vars vars, const rec_data &rec, const multi_thresh_indeces &mtIndeces, const int &nWindow, int i) {
	fft_analysis fftAnal;

	// SPLIT up sound file
	for (int j = 0; j < rec.channels * nWindow; j += rec.channels) {
		vars.window[j >> (rec.channels >> 1)] = rec.samples[i*nWindow + j]; // Ignore all channels but channel 0 
	}
	fftw_execute(vars.p); // Repeatable

	// Obtain absolute, normalised FFT
	vars.absFFT[0] = vars.out[0][0] / nWindow;
	for (long j = 1; j < (nWindow / 2 - 1); j++) {
		vars.absFFT[j] = 2 * sqrt(pow(vars.out[j][0], 2) + pow(vars.out[j][1], 2)) / nWindow;
	}

	// Obtain noise levels
	double totalNoise = 0;
	for (long j = mtIndeces.noiseIndexLowMin; j < mtIndeces.noiseIndexLowMax; j++) {
		totalNoise += vars.absFFT[j];
	}
	for (long j = mtIndeces.noiseIndexHighMin; j < mtIndeces.noiseIndexHighMax; j++) {
		totalNoise += vars.absFFT[j];
	}
	fftAnal.noiseThresh = noiseMultiplier * totalNoise / ((mtIndeces.noiseIndexLowMax - mtIndeces.noiseIndexLowMin) 
		+ (mtIndeces.noiseIndexHighMax - mtIndeces.noiseIndexHighMin));

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
		printf(" %f ", fftAnal.bandAvgs[j]);
	}
	printf("\n");

	return fftAnal;
}


void detect(const fft_analysis &fftAnal, int(&detectedBands)[BANDS]) {
	int detections = 0;

	for (int i = 0; i < BANDS; i++) {
		detectedBands[i] = (fftAnal.bandAvgs[i] >= fftAnal.noiseThresh);
	}
	for (int i = 0; i < BANDS; i++)	{
		detections += detectedBands[i];
	}
	printf("The siren is present in %d out of %d bands, and they are:", detections, BANDS);

	for (int j = 0; j < BANDS; j++)	{
		printf(" %d ", detectedBands[j]);
	}
	printf("\n");
}

// Compare only parent windows
void directionParentOnly(const fft_analysis *fftAnals, const int (&detectedBands)[W][BANDS]) {
	double windowAvgs[W] = { 0 };

	for (int i = 0; i < W; i++) {
		for (int j = 0; j < BANDS; j++) {
			windowAvgs[i] += (fftAnals[i].bandAvgs[j] * detectedBands[i][j]);
		}
		windowAvgs[i] = windowAvgs[i] / BANDS;
	}

	for (int i = 1; i < W; i++) {
		double rel = windowAvgs[i] / windowAvgs[i - 1];
		if (rel > (1 + dirMargin)) {
			printf("Detected EV is approaching at %f. \n", rel);
		}
		else if (rel < (1 - dirMargin)) {
			printf("Detected EV is moving away at %f. \n", rel);
		}
		else {
			printf("Detected direction is inconclusive at %f. \n", rel);
		}
	}
}

// Compare all subwindows but only in parent detected bands
void directionSubParentBands(const fft_analysis *fftSubs, const int *detectedBands, const int &i) {
	double windowAvgs[SPLIT] = { 0 };
	printf("Window %d direction: \n", i);
	for (int j = 0; j < SPLIT; j++) {
		for (int k = 0; k < BANDS; k++) {
			windowAvgs[j] += (fftSubs[SPLIT*i+j].bandAvgs[k] * detectedBands[k]);
		}
		windowAvgs[j] = windowAvgs[j] / BANDS;
		printf(" %f ", windowAvgs[j]);
	}
	printf("\n");
}

// Compare all subwindows, in all bands (and with doppler-ignoring bands)
// NB: Currently *accumulating* subwindow averages
void directionSubAllBands(const fft_analysis *fftSubs, const int &i) {
	double windowAvgs[SPLIT] = { 0 };
	double windowAvgsTotal = 0;
	double windowAvgsAccum[SPLIT] = { 0 };

	printf("Window %d direction: \n", i);
	for (int j = 0; j < SPLIT; j++) {
		for (int k = 0; k < BANDS; k++) {
			windowAvgs[j] += (fftSubs[SPLIT*i + j].bandAvgs[k]);
		}
		windowAvgs[j] = windowAvgs[j] / BANDS;
		windowAvgsTotal += windowAvgs[j];
		windowAvgsAccum[j] = windowAvgsTotal / (j + 1);
		printf(" %f ", windowAvgs[j]);
	}
	printf("\n");

	int dir = 0;
	
	for (int j = 1; j < SPLIT; j++) {
		double rel = windowAvgsAccum[j] / windowAvgsAccum[j - 1];
		if (rel > (1 + dirMargin)) {
			dir++;
		}
		if (rel < (1 - dirMargin)) {
			dir--;
		}		
	}
	printf("Detected EV is moving in direction %d \n", dir);
}

int main()
{
	// Read recording
	rec_data recording = readRecording("yt_yelp_approaching.wav");

	// --------------------PARENT----------------\\
	// Set up Multi-thresholding
	int nWindow = fullWindow * recording.fs;
	multi_thresh_indeces mtIndeces = setupMultiThresholding(nWindow, recording.fs, true); // Account doppler

	fft_vars fftV = setupFFT(nWindow); // DO NOT RUN THIS AGAIN

	// Obtain FFT-analysis
	fft_analysis fftAnals[W];
	int detectedBands[W][BANDS];
	for (int i = 0; i < W; i++) {
		fftAnals[i] = doFFT(fftV, recording, mtIndeces, nWindow, i);
		// Detection
		detect(fftAnals[i], detectedBands[i]);
	}

	// Direction
	directionParentOnly(fftAnals, detectedBands);

	//---------------------SUB---------------------\\
	// Set up Multi-thresholding
	int nSubWindow = subWindow * recording.fs;
	multi_thresh_indeces mtIndecesDir = setupMultiThresholding(nSubWindow, recording.fs, false); // Ignore doppler

	// Obtain FFT-analysis
	fft_analysis fftAnalsDir[SW];
	int detectedBandsDir[SW][BANDS];
	for (int i = 0; i < SW; i++) {
		fftAnalsDir[i] = doFFT(fftV, recording, mtIndecesDir, nSubWindow, i);
	}

	// Direction
	for (int i = 0; i < W; i++) {
		//directionSubParentBands(fftAnalsDir, detectedBands[i], i);
		directionSubAllBands(fftAnalsDir, i);
	}

	free(recording.samples);    //in is destroyed by plan execution
	fftw_destroy_plan(fftV.p);
	fftw_free(fftV.out);
	free(fftV.absFFT);

	printf("Test was succesful. Somewhat. \n");

	system("PAUSE");
	return 0;
}

