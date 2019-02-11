// ConsoleApplication1.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
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
const double DOPPLER_MAX = 1.1777;
// Define doppler-adjusted frequency band of interest
const double threshLow = 700 * DOPPLER_MAX;
const double threshHigh = 1500 * DOPPLER_MIN;
// Frequencies for multithresholding
const double noiseThreshLowMin = 150;
const double noiseThreshLowMax = 510;
const double noiseThreshHighMin = 1885;
const double noiseThreshHighMax = 3000;
// Number of bands for multithresholding
const int BANDS = 4;
const int noiseMultiplier = 3;

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
	const char *fName = "passing_yelp_city.WAV";

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

	// Set up multithresholding
	int bandIndeces[BANDS+1] = { 0 };   // Band limit values (fft index)
	int detection[BANDS] = { 0 }; // Multithreshold detection results
	// Find array indeces
	double df = (double)fs / (double)n;
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

	printf("There are %d channels, %d frames, rate is %d, and df=%f \n", channels, n, fs, df);
	printf("The indeces are %d, %d, %d, %d, %d \n", bandIndeces[0], bandIndeces[1], bandIndeces[2], bandIndeces[3], bandIndeces[4]);

	// Set up FFT
	double *in; 
	fftw_complex *out;   // Complex 1D, n/2-1 length output
	fftw_plan p;
	in = (double*) malloc(sizeof(double) * 2 * (n / 2 - 1));
	out = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * 2 * (n / 2 - 1));   // In-place fft requires in and out to accomodate two out-arrays
	p = fftw_plan_dft_r2c_1d(n, in, out, FFTW_ESTIMATE); // MEASURE consumes extra time on initial plan execution. ESTIMATE has no initial timecost.
	
	// Fill input from data
	for (long i = 0; i < (channels*n); i+=channels) {
		in[i >> (channels>>1)] = data[i];   // Ignore all channels but channel 0 NB: CANT HAVE UNEVEN NUMBER OF CHANNELS
	}

	fftw_execute(p); // Repeatable

	// Check output
	writeToFile("fftresults.txt", out);

	// Evaluate output
	// Obtain absolute, normalised FFT
	double *absFFT = (double*)malloc(sizeof(double) * (n / 2 - 1));
	absFFT[0] = out[0][0]/n;
	for (long i = 1; i < (n / 2 - 1); i++) {
		absFFT[i] = 2*sqrt(pow(out[i][0], 2) + pow(out[i][1],2))/n;
	}
	// Obtain noise levels
	double totalNoise = 0;
	for (long i = noiseIndexLowMin; i < noiseIndexLowMax; i++) {
		totalNoise += absFFT[i];
	}
	for (long i = noiseIndexHighMin; i < noiseIndexHighMax; i++) {
		totalNoise += absFFT[i];
	}
	double noiseThresh = noiseMultiplier * totalNoise / ((noiseIndexLowMax - noiseIndexLowMin) + (noiseIndexHighMax - noiseIndexHighMin));
	// Obtain BoI (Bands of Interest) levels
	double totalVol[BANDS] = { 0 };
	double avgVol[BANDS] = { 0 };
	int detections = 0;
	for (int i = 0; i < BANDS; i++) {
		for (int j = bandIndeces[i]; j < bandIndeces[i + 1]; j++) {
			totalVol[i] += absFFT[j];
		}
		avgVol[i] = totalVol[i] / bandLength;
		detection[i] = (avgVol[i] >= noiseThresh);
		detections += detection[i];
	}

	printf("The noise threshold is %f, the band averages are: %f, %f, %f, %f \n", noiseThresh, avgVol[0], avgVol[1], avgVol[2], avgVol[3]);
	printf("The siren is present in %d out of %d bands \n", detections, BANDS);


	fftw_destroy_plan(p);
	fftw_free(out); free(data); free(absFFT);   //in is destroyed by plan execution
	
	printf("Test was succesful. Somewhat. \n");

	system("PAUSE");
	return 0;
}

