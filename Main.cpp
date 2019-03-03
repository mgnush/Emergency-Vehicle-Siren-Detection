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
const double threshLow = 700 * DOPPLER_MAX;
const double threshHigh = 1500 * DOPPLER_MIN;
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
const int SPLIT = 4;
const double subWindow = fullWindow / SPLIT;
const int SW = SPLIT * W;

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

	printf("There are %d channels, %d frames, rate is %d. \n \n", data.channels, data.n, data.fs);

	return data;
}

multi_thresh_indeces setupMultiThresholding(const int &n, const int &fs) {

	multi_thresh_indeces mtIndeces;

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
	vars.out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (nWindow / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays   // Complex 1D, n/2-1 length output
	vars.p = fftw_plan_dft_r2c_1d(nWindow, vars.window, vars.out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
	vars.absFFT = (double*)malloc(sizeof(double) * (nWindow / 2 - 1));

	return vars;
}

fft_analysis doFFT(fft_vars vars, const rec_data &rec, const multi_thresh_indeces &mtIndeces, const int &nWindow, int i) {
	fft_analysis fftAnal;
	// SPLIT up sound file
	for (int j = 0; j < rec.channels * nWindow; j += rec.channels) {
		vars.window[j >> (rec.channels >> 1)] = rec.samples[i*nWindow + j]; // Ignore all channels but channel 0 NB: CANT HAVE UNEVEN NUMBER OF CHANNELS
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
	fftAnal.noiseThresh = noiseMultiplier * totalNoise / ((mtIndeces.noiseIndexLowMax - mtIndeces.noiseIndexLowMin) + (mtIndeces.noiseIndexHighMax - mtIndeces.noiseIndexHighMin));

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

	for (int i = 0; i < BANDS; i++)
	{
		detections += detectedBands[i];
	}
	printf("The siren is present in %d out of %d bands, and they are:", detections, BANDS);
	for (int j = 0; j < BANDS; j++)
	{
		printf(" %d ", detectedBands[j]);
	}
	printf("\n");
}

int main()
{
	// Read recording
	rec_data recording = readRecording("police_yelp_rec.wav");

	// --------------------DETECTION----------------\\
	// Set up Multi-thresholding
	printf("\nParent windows: \n \n");
	int nWindow = fullWindow * recording.fs;
	multi_thresh_indeces mtIndeces = setupMultiThresholding(nWindow, recording.fs);

	fft_vars fftV = setupFFT(nWindow); // DO NOT RUN THIS AGAIN

	// Obtain FFT-analysis
	fft_analysis fftAnals[W];
	int detectedBands[W][BANDS];

	for (int i = 0; i < W; i++) {
		fftAnals[i] = doFFT(fftV, recording, mtIndeces, nWindow, i);
		// Detection
		detect(fftAnals[i], detectedBands[i]);
	}

	//---------------------DIRECTION---------------------\\
	// Set up Multi-thresholding
	printf("\nSubwindows for direction: \n \n");
	int nSubWindow = subWindow * recording.fs;
	multi_thresh_indeces mtIndecesDir = setupMultiThresholding(nSubWindow, recording.fs);

	// Obtain FFT-analysis
	fft_analysis fftAnalsDir[SW];
	int detectedBandsDir[SW][BANDS];

	for (int i = 0; i < SW; i++) {
		fftAnalsDir[i] = doFFT(fftV, recording, mtIndecesDir, nSubWindow, i);
		// Detection
		detect(fftAnalsDir[i], detectedBandsDir[i]);
	}

	int parentBandsDetected;
	int count = 0;
	double Upper_Limit = 1.10;
	double Lower_Limit = 0.90;

	for (int i = 0; i < W; i++) {
		parentBandsDetected = 0;
		for (int j = 0; j < BANDS; j++) {
			if (detectedBands[i][j])
			{
				printf(" Band %d is detected \"%d\" thus direction is run on split windows\n", j, detectedBands[i][j]);

				for (int k = 1; k < SW; k++)
				{
						// taking the ratio of the 1st window and second window and then comparing it to the limits,
						// if they are within the limit the iteration is skipped, if not the counter is indexed to reflect that
						double PrevOverPres = (fftAnalsDir[k - 1].bandAvgs[j] / fftAnalsDir[k].bandAvgs[j]);
						double PresOverPrev = (fftAnalsDir[k].bandAvgs[j] / fftAnalsDir[k - 1].bandAvgs[j]);
						if ((PrevOverPres < Upper_Limit) && (PrevOverPres > Lower_Limit))
						{
							//printf("Skipped because %f \n", PrevOverPres);
							continue;
						}
						if ((PresOverPrev < Upper_Limit) && (PresOverPrev > Lower_Limit))
						{
							//printf("Skipped because %f \n", PresOverPrev);
							continue;
						}
						if (PrevOverPres > PresOverPrev)
						{
							//printf("Sound is Getting Lower \n");
							count--;
						}
						if (PrevOverPres < PresOverPrev)
						{
							//printf("Sound is Getting Louder \n");
							count++;
						}
				}
			}
		}

		printf("%d bands were detected in window %d \n", parentBandsDetected, i);
	}
	
	// setting up a counter to determine wether moving away or towards, the limits are to see if the 2 window averages
	// are within a certain percentage of each other
	
	if (parentBandsDetected > 2 )
	{
		// printf("test \n");
		
		printf("Due to amount of splits, individual results are omitted. \n");
		// counter is then compared to see wether it is moving towards or away from mic
		if (count > 0)
		{
			printf("EV is moving towards you D:\n");
		}
		else if (count < 0)
		{
			printf("EV is moving away from you :D\n");
		}
		else
		{
			printf("Result is inconclusive can't tell if moving away or towards you. Maybe you are the EV :o\n");
		}
	}
	
	free(recording.samples);    //in is destroyed by plan execution
	fftw_destroy_plan(fftV.p);
	fftw_free(fftV.out);
	free(fftV.absFFT);

	printf("Test was succesful. Somewhat. \n");

	system("PAUSE");
	return 0;
}
