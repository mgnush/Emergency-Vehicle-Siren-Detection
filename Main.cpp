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
const int SPLIT = 2;
const double nSubWindow = fullWindow / SPLIT;
const int SW = SPLIT * W;

struct rec_data {
	int channels;
	long n;
	int fs;
	double *samples;
};

struct multiThresholdIndeces {
	int bandIndeces[BANDS + 1];
	int bandLength;
	int noiseIndexLowMin;
	int noiseIndexLowMax;
	int noiseIndexHighMin;
	int noiseIndexHighMax;
};

struct fftAnalysis {
	double bandAvgs[BANDS];
	double noiseThresh;
};

void writeToFile(const char* fileName, fftw_complex *&data)
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
	const char *fName = "police_wail_rec.WAV";

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

multiThresholdIndeces setupMultiThresholding(const int &n, const int &fs) {

	multiThresholdIndeces mtIndeces;

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

fftAnalysis doFFT(const rec_data &rec, const multiThresholdIndeces &mtIndeces, const int &nWindow, int i) {
	fftAnalysis fftAnal;
	double *window;
	fftw_complex *out;   // Complex 1D, n/2-1 length output
	fftw_plan p;
	double *absFFT;

	window = (double*)malloc(sizeof(double) * 2 * (nWindow / 2 - 1));
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (nWindow / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays
	p = fftw_plan_dft_r2c_1d(nWindow, window, out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
	absFFT = (double*)malloc(sizeof(double) * (nWindow / 2 - 1));
	// SPLIT up sound file
	for (int j = 0; j < rec.channels * nWindow; j += rec.channels) {
		window[j >> (rec.channels >> 1)] = rec.samples[i*nWindow + j]; // Ignore all channels but channel 0 NB: CANT HAVE UNEVEN NUMBER OF CHANNELS
	}

	fftw_execute(p); // Repeatable

	// Obtain absolute, normalised FFT
	absFFT[0] = out[0][0] / nWindow;
	for (long j = 1; j < (nWindow / 2 - 1); j++) {
		absFFT[j] = 2 * sqrt(pow(out[j][0], 2) + pow(out[j][1], 2)) / nWindow;
	}

	// Obtain noise levels
	double totalNoise = 0;
	for (long j = mtIndeces.noiseIndexLowMin; j < mtIndeces.noiseIndexLowMax; j++) {
		totalNoise += absFFT[j];
	}
	for (long j = mtIndeces.noiseIndexHighMin; j < mtIndeces.noiseIndexHighMax; j++) {
		totalNoise += absFFT[j];
	}

	fftAnal.noiseThresh = noiseMultiplier * totalNoise / ((mtIndeces.noiseIndexLowMax - mtIndeces.noiseIndexLowMin) + (mtIndeces.noiseIndexHighMax - mtIndeces.noiseIndexHighMin));

	// Obtain BoI (Bands of Interest) levels
	double totalVol[BANDS] = { 0 };

	for (int j = 0; j < BANDS; j++) {
		for (int k = mtIndeces.bandIndeces[j]; k < mtIndeces.bandIndeces[j + 1]; k++) {
			totalVol[j] += absFFT[k];
		}
		fftAnal.bandAvgs[j] = totalVol[j] / mtIndeces.bandLength;
	}
	fftw_destroy_plan(p);
	fftw_free(out);
	free(absFFT);

	return fftAnal;
}

int main()
{
	// Read recording
	rec_data recording = readRecording("police_yelp_rec.wav");

	// Set up Multi-thresholding
	int nWindow = fullWindow * recording.fs;
	multiThresholdIndeces mtIndeces = setupMultiThresholding(nWindow, recording.fs);

	// Obtain FFT-analysis
	fftAnalysis fftAnals[W];

	for (int i = 0; i < W; i++) {
		fftAnals[i] = doFFT(recording, mtIndeces, nWindow, i);
		// Print for testing
		printf("Window %d: The noise threshold is %f, and the band averages are", i, fftAnals[i].noiseThresh);
		for (int j = 0; j < BANDS; j++) {
			printf(" %f ", fftAnals[i].bandAvgs[j]);
		}
		printf("\n");
	}


	//writeToFile("fft0.txt", out[0]);
	//writeToFile("fft1.txt", out[1]);

	// Detection


	/*
	// set up FFT for the direction windows
	fftw_complex *outDirection[SW];   // Complex 1D, n/2-1 length output
	fftw_plan pDirction[SW];
	double *absFFTDirection[SW];
	for (int i = 0; i < SW; i++) {
		windowsForDirection[i] = (double*)malloc(sizeof(double) * 2 * (int)(nWindowDirection / 2 - 1));
		outDirection[i] = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (nWindowDirection / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays
		pDirction[i] = fftw_plan_dft_r2c_1d(nWindowDirection, windowsForDirection[i], outDirection[i], FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
		absFFTDirection[i] = (double*)malloc(sizeof(double) * (nWindowDirection / 2 - 1));
		// SPLIT up sound file
		for (int j = 0; j < channels * nWindowDirection; j += channels) {
			windowsForDirection[i][j >> (channels >> 1)] = data[(i*nWindowDirection) + j]; // Ignore all channels but channel 0 NB: CANT HAVE UNEVEN NUMBER OF CHANNELS
		}
	}
	for (int i = 0; i < SW; i++) {
		fftw_execute(pDirction[i]); // Repeatable
	}
	*/

	/*
	writeToFile("fft0direction.txt", outDirection[0]);
	writeToFile("fft1direction.txt", outDirection[1]);
	*/

	/*

	// Detection
	double avgVol[W][BANDS];
	double avgVolDirection[SW][BANDS];

	int check = 0;
>>>>>>> Stashed changes

	for (int i = 0; i < W; i++) {
		// Obtain absolute, normalised FFT
		absFFT[i][0] = out[i][0][0] / nWindow;
		for (long j = 1; j < (nWindow / 2 - 1); j++) {
			absFFT[i][j] = 2 * sqrt(pow(out[i][j][0], 2) + pow(out[i][j][1], 2)) / nWindow;
		}
		// Obtain noise levels
		double totalNoise = 0;
		for (long j = noiseIndexLowMin; j < noiseIndexLowMax; j++) {
			totalNoise += absFFT[i][j];
		}
		for (long j = noiseIndexHighMin; j < noiseIndexHighMax; j++) {
			totalNoise += absFFT[i][j];
		}
		double noiseThresh = noiseMultiplier * totalNoise / ((noiseIndexLowMax - noiseIndexLowMin) + (noiseIndexHighMax - noiseIndexHighMin));
		// Obtain BoI (Bands of Interest) levels
		double totalVol[BANDS] = { 0 };
		int detections = 0;
		for (int j = 0; j < BANDS; j++) {
			for (int k = bandIndeces[j]; k < bandIndeces[j + 1]; k++) {
				totalVol[j] += absFFT[i][k];
			}
			avgVol[i][j] = totalVol[j] / bandLength;
			detection[j] = (avgVol[i][j] >= noiseThresh);
			detections += detection[j];
		}
		printf("Window %d: The noise threshold is %f, the band averages are:", i, noiseThresh);
		for (int j = 0; j < BANDS; j++) {
			printf(" %f ", avgVol[i][j]);
		}
		printf("\n");
		printf("The siren is present in %d out of %d bands \n", detections, BANDS);
	}
<<<<<<< Updated upstream

	free(data);    //in is destroyed by plan execution
	for (int i = 0; i < W; i++) {
		fftw_destroy_plan(p[i]);
		fftw_free(out[i]);
		free(absFFT[i]);
	}

=======
	/*
	if (check = 1)
	{
		// detection direction
		for (int i = 0; i < SW; i++)
		{
			// Obtain absolute, normalised FFT for direction FFT
			absFFTDirection[i][0] = outDirection[i][0][0] / nWindowDirection;
			for (long j = 1; j < (nWindowDirection / 2 - 1); j++) {
				absFFTDirection[i][j] = 2 * sqrt(pow(outDirection[i][j][0], 2) + pow(outDirection[i][j][1], 2)) / nWindowDirection;
			}
			// Obtain noise levels for direction
			double totalNoisedirection = 0;
			for (long j = noiseIndexLowMin; j < noiseIndexLowMax; j++) {
				totalNoisedirection += absFFTDirection[i][j];
			}
			for (long j = noiseIndexHighMin; j < noiseIndexHighMax; j++) {
				totalNoisedirection += absFFTDirection[i][j];
			}


			double noiseThreshDirection = noiseMultiplier * totalNoisedirection / ((noiseIndexLowMax - noiseIndexLowMin) + (noiseIndexHighMax - noiseIndexHighMin));
			// Obtain BoI (Bands of Interest) levels for direction
			double totalVolDirection[BANDS] = { 0 };
			int detectionsDirection = 0;
			for (int j = 0; j < BANDS; j++) {
				for (int k = bandIndeces[j]; k < bandIndeces[j + 1]; k++) {
					totalVolDirection[j] += absFFTDirection[i][k];
				}
				avgVolDirection[i][j] = totalVolDirection[j] / bandLength;
				detectionDirection[j] = (avgVolDirection[i][j] >= noiseThreshDirection);
				detectionsDirection += detectionDirection[j];
			}

			printf("Window %d: The noise threshold  is %f, the band direction averages are:", i, noiseThreshDirection);
			for (int j = 0; j < BANDS; j++) {
				printf(" %f ", avgVolDirection[i][j]);
			}
			printf("\n");
			printf("The siren is present in direction %d out of %d bands \n", detectionsDirection, BANDS);
			if (detectionsDirection >= (BANDS - 2))
			{
				printf("Number of direction deteced bands is greater than or equal %d bands of the original %d bands. \n", BANDS - 2, BANDS);

			}
		}
	}
	*/
	free(recording.samples);    //in is destroyed by plan execution

	/*
	for (int i = 0; i < SW; i++) {
		fftw_destroy_plan(pDirction[i]);
		fftw_free(outDirection[i]);
		free(absFFTDirection[i]);
	}
	*/

	printf("Test was succesful. Somewhat. \n");

	system("PAUSE");
	return 0;
}

