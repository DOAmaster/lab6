//
//Gordon Griesel
//Fall 2016
//
//Convert SRTM data into a grayscale heightmap
//Shuttle Radar Topography Mission
//https://www2.jpl.nasa.gov/srtm/
//
#include <iostream>
#include <stdio.h>
using namespace std;

int main()
{
	const char filename[] = "N35W119";
	char hgt[100];
	char ppm[100];
	sprintf(hgt, "%s.hgt", filename);
	sprintf(ppm, "%s.ppm", filename);
	//FILE *fpi = fopen("N32W108.hgt","r");
	//FILE *fpo = fopen("N32W108.ppm","w");
	FILE *fpi = fopen(hgt,"r");
	FILE *fpo = fopen(ppm,"w");
	if (!fpi) {
		printf("ERROR opening **%s** for input\n", hgt); fflush(stdout);
		return 0;
	}
	if (!fpo) {
		printf("ERROR opening **%s** for output\n", ppm); fflush(stdout);
		return 0;
	}
	printf("file **%s** opened.\n", hgt); fflush(stdout);
	printf("file **%s** opened.\n", ppm); fflush(stdout);
	fprintf(fpo,"P3\n1201 1201\n255\n");
	int b0, b1;
	int low = 9999999;
	int hig = -9999999;
	for (int i=0; i<1201; i++) {
		for (int j=0; j<1201; j++) {
			b0 = fgetc(fpi);
			b1 = fgetc(fpi);
			int val = b0;
			val <<= 8;
			val += b1;
			if (val >= 32700) val = 1;
			if (low > val) low = val;
			if (hig < val) hig = val;
		}
	}
	printf("low: %i\n",low);
	printf("high: %i\n",hig);
	fseek(fpi, 0, SEEK_SET);
	for (int i=0; i<1201; i++) {
		for (int j=0; j<1201; j++) {
			b0 = fgetc(fpi);
			b1 = fgetc(fpi);
			int val = b0;
			val <<= 8;
			val += b1;
			if (val >= 32700) val = 1;
			//val -= low;
			float fval = (float)val;
			float div = hig/255.0;
			val = (int)(fval / div); 
			fprintf(fpo, "%3i %3i %3i\n", val, val, val);
		}
	}
	fclose(fpo);
	fclose(fpi);
	printf("Success.\nLook for a png image named...\n");
	//Enter some code to save a png or gif image.
	//Do a conversion using Imagemagick.


	return 0;
}

