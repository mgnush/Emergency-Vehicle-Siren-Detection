// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
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
const int Split = 2;
const double splitwindowsize = fullWindow / Split;
const int NumberofWindowsDirection = Split * W;


void writeToFile(const char* fileName, fftw_complex *&data)
{
	std::ofstream myFile;
	myFile.open(fileName);

	for (int i = 0; i < 5; i++) {
		myFile << data[i][0] << ", " << data[i][1] << "\n";
	}
	myFile.close();
}

int main()
{
	// Open soundfile
	SNDFILE *policeSiren;
	SF_INFO sfinfo;
	const char *fName = "police_yelp_rec.wav";

	if (!(policeSiren = sf_open(fName, SFM_READ, &sfinfo))) {
		printf("Not able to open sound file %s \n", fName);
		puts(sf_strerror(NULL));
		return 1;
	}

	int channels = sfinfo.channels;
	long n = sfinfo.frames;
	int fs = sfinfo.samplerate;

	// Fetch sound data
	double *data;
	data = (double*)malloc(sizeof(double) * channels * n);
	sf_count_t itemsExpected = channels * n;
	sf_count_t items = sf_read_double(policeSiren, data, itemsExpected);

	if (itemsExpected != items) {
		printf("File data error \n");
		return 1;
	}
	sf_close(policeSiren);


	int nWindow = fullWindow * fs;
	double *windows[W];

	int nWindowDirection = splitwindowsize * fs;
	double *windowsForDirection[NumberofWindowsDirection];

	// Set up multithresholding
	int bandIndeces[BANDS + 1] = { 0 };   // Band limit values (fft index)
	int detection[BANDS] = { 0 }; // Multithreshold detection results


	int detectionDirection[BANDS] = { 0 }; // Multithreshold detection results

	// Find array indeces
	double df = (double)fs / (double)nWindow;
	bandIndeces[0] = (int)(threshLow / df);
	bandIndeces[BANDS] = (int)(threshHigh / df);
	int bandLength = (bandIndeces[BANDS] - bandIndeces[0]) / BANDS;
	for (int i = 1; i < BANDS; i++) {
		bandIndeces[i] = bandIndeces[i - 1] + bandLength;
	}
	int noiseIndexLowMin = (int)(noiseThreshLowMin / df);
	int noiseIndexLowMax = (int)(noiseThreshLowMax / df);
	int noiseIndexHighMin = (int)(noiseThreshHighMin / df);
	int noiseIndexHighMax = (int)(noiseThreshHighMax / df);

	printf("There are %d channels, %d frames, rate is %d, and df=%f. Window length is %d \n", channels, n, fs, df, nWindow);
	//printf("The indeces are %d, %d, %d, %d, %d \n", bandIndeces[0], bandIndeces[1], bandIndeces[2], bandIndeces[3], bandIndeces[4]);

	// Set up FFT
	fftw_complex *out[W];   // Complex 1D, n/2-1 length output
	fftw_plan p[W];
	double *absFFT[W];
	for (int i = 0; i < W; i++) {
		windows[i] = (double*)malloc(sizeof(double) * 2 * (nWindow / 2 - 1));
		out[i] = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (nWindow / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays
		p[i] = fftw_plan_dft_r2c_1d(nWindow, windows[i], out[i], FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
		absFFT[i] = (double*)malloc(sizeof(double) * (nWindow / 2 - 1));
		// Split up sound file
		for (int j = 0; j < channels * nWindow; j += channels) {
			windows[i][j >> (channels >> 1)] = data[(i*nWindow) + j]; // Ignore all channels but channel 0 NB: CANT HAVE UNEVEN NUMBER OF CHANNELS
		}
	}
	for (int i = 0; i < W; i++) {
		fftw_execute(p[i]); // Repeatable
	}
	/*
	// set up FFT for the direction windows
	fftw_complex *outDirection[NumberofWindowsDirection];   // Complex 1D, n/2-1 length output
	fftw_plan pDirction[NumberofWindowsDirection];
	double *absFFTDirection[NumberofWindowsDirection];
	for (int i = 0; i < NumberofWindowsDirection; i++) {
		windowsForDirection[i] = (double*)malloc(sizeof(double) * 2 * (int)(nWindowDirection / 2 - 1));
		outDirection[i] = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (nWindowDirection / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays
		pDirction[i] = fftw_plan_dft_r2c_1d(nWindowDirection, windowsForDirection[i], outDirection[i], FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
		absFFTDirection[i] = (double*)malloc(sizeof(double) * (nWindowDirection / 2 - 1));
		// Split up sound file
		for (int j = 0; j < channels * nWindowDirection; j += channels) {
			windowsForDirection[i][j >> (channels >> 1)] = data[(i*nWindowDirection) + j]; // Ignore all channels but channel 0 NB: CANT HAVE UNEVEN NUMBER OF CHANNELS
		}
	}
	for (int i = 0; i < NumberofWindowsDirection; i++) {
		fftw_execute(pDirction[i]); // Repeatable
	}
	*/
	writeToFile("fft0.txt", out[0]);
	writeToFile("fft1.txt", out[1]);

	/*
	writeToFile("fft0direction.txt", outDirection[0]);
	writeToFile("fft1direction.txt", outDirection[1]);
	*/

	

	// Detection
	double avgVol[W][BANDS];
	double avgVolDirection[NumberofWindowsDirection][BANDS];

	int check = 0;

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
		if (detections >= (BANDS/2))
		{
			check = 1;
			printf("Number of deteced bands is greater than or equal %d bands of the original %d bands. \n", BANDS/2, BANDS);

		}

	}

	double Upper_Percentage_LIMIT = 1.10;
	double Lower_Percentage_LIMIT = 0.90;
	if (check)
	{
		int count = 0;

		for (int i = 1; i < W; i++)
		{
			for (int j = 0; j < BANDS; j++)
			{
				double FirstPeriod_over_Second = (avgVol[i-1][j] / avgVol[i][j]);
				double SecondPeriod_over_First = (avgVol[i][j] / avgVol[i-1][j]);

				if ((FirstPeriod_over_Second < Upper_Percentage_LIMIT) && (FirstPeriod_over_Second > Lower_Percentage_LIMIT))
				{
					printf("Skipped because %f \n", (FirstPeriod_over_Second));
					continue;
				}
				if ((SecondPeriod_over_First < Upper_Percentage_LIMIT) && (SecondPeriod_over_First > Lower_Percentage_LIMIT))
				{
					printf("Skipped because %f \n", (SecondPeriod_over_First));
					continue;
				}

				if (avgVol[i - 1][j] > avgVol[i][j])
				{
					printf("sound is getting lower \n");
					count--;
				}
				if (avgVol[i - 1][j] < avgVol[i][j])
				{
					printf("sound is getting louder \n");
					count++;
				}

			}
		}
		printf("count is %d \n", count);
		if (count >= 1)
		{
			printf("EV is moving towards you D: \n");
		}
		if (count <= -1)
		{
			printf("EV is moving away from you :D \n");
		}
	}
	/*
	if (check = 1)
	{
		// detection direction
		for (int i = 0; i < NumberofWindowsDirection; i++)
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
	free(data);    //in is destroyed by plan execution
	for (int i = 0; i < W; i++) {
		fftw_destroy_plan(p[i]);
		fftw_free(out[i]);
		free(absFFT[i]);
	}

	/*
	for (int i = 0; i < NumberofWindowsDirection; i++) {
		fftw_destroy_plan(pDirction[i]);
		fftw_free(outDirection[i]);
		free(absFFTDirection[i]);
	}
	*/
	printf("Test was succesful. Somewhat. \n");

	system("PAUSE");
	return 0;
}
