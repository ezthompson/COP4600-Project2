#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>

#define BUFFER_SIZE 1048576 // 1MB

int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

struct fileStruct {
	char* files[10];
	char* dir;
	int in;
	int out;
	FILE* fOut;
};

pthread_mutex_t lock;
void *compressVideo(void* args) {
	//Cast args to a struct pointer so we can obtain the data
	struct fileStruct *threadArgs = (struct fileStruct*) args;
	for(int i = 0; i < 10; i++) {
		//Create a string length variable equal to the length of the directory + the file name + 2
		int len = strlen(threadArgs -> dir)+strlen(threadArgs -> files[i])+2;
		//Create a string that will contain the complete file path of the frame
		char *full_path = malloc(len*sizeof(char));
		assert(full_path != NULL);
		strcpy(full_path, threadArgs -> dir);
		strcat(full_path, "/");
		strcat(full_path, threadArgs -> files[i]);

		//Create buffer arrays to contain data for original and compressed frame data
		unsigned char buffer_in[BUFFER_SIZE];
		unsigned char buffer_out[BUFFER_SIZE];

		// load file
		FILE *f_in = fopen(full_path, "r");
		assert(f_in != NULL);
		//Read in the uncompressed frame data
		int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in);
		fclose(f_in);
		threadArgs -> in += nbytes;

		// zip file
		z_stream strm;
		int ret = deflateInit(&strm, 9);
		assert(ret == Z_OK);
		strm.avail_in = nbytes;
		strm.next_in = buffer_in;
		strm.avail_out = BUFFER_SIZE;
		strm.next_out = buffer_out;

		ret = deflate(&strm, Z_FINISH);
		assert(ret == Z_STREAM_END);

		// dump zipped file
		int nbytes_zipped = BUFFER_SIZE-strm.avail_out;
		fwrite(&nbytes_zipped, sizeof(int), 1, threadArgs -> fOut);
		fwrite(buffer_out, sizeof(unsigned char), nbytes_zipped, threadArgs -> fOut);
		threadArgs -> out += nbytes_zipped;

		free(full_path);
	}
}

int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!

	assert(argc == 2);

	//Represents a directory stream
	DIR *d;
	//Dir struct
	struct dirent *dir;
	//List of frame names
	char **files = NULL;
	int nfiles = 0;

	//Attempt to open the directory specified by the second cmd line argument and print an error if unsuccessful
	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		files = realloc(files, (nfiles+1)*sizeof(char *));
		assert(files != NULL);

		//Get the length of the filename
		int len = strlen(dir->d_name);
		//If the current file in the directory is a .ppm file
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			//Add the current frame to files and check that it is a valid entry
			files[nfiles] = strdup(dir->d_name);
			assert(files[nfiles] != NULL);

			//Increment nfiles by 1
			nfiles++;
		}
	}
	//Close the directory stream
	closedir(d);
	//Perform quick sort on files using cmp as the sorting method, returning the files array sorted in ascending order
	qsort(files, nfiles, sizeof(char *), cmp);

	// create a single zipped package with all PPM files in lexicographical order
	int total_in = 0, total_out = 0;
	//Create a file variable named "video.vzip" with write permissions
	FILE *f_out = fopen("video.vzip", "w");
	//Check that the creation of f_out was successful
	assert(f_out != NULL);

	//Create an array of ten threads
	pthread_t threads[10];

	//Loop through the files and divide the compression into groups of ten
	for (int i = 0; i < 10; i++) {
		//Create string array of the current 10 files
		char* fileSect[10];
		int inP, outP = 0;
		//Creating a struct that will contain the necessary data to pass to compressVideo
		struct fileStruct *threadArgs = malloc(sizeof(struct fileStruct));
		//Add the current 10 files in files to fileSect
		for (int j = 0; j < 10; j++) {
			threadArgs -> files[j] = files[(i * 10) + j];
		}
		
		assert(threadArgs != NULL);

		//Assigning each variable of the struct
		threadArgs -> dir = argv[1];
		threadArgs -> in = inP;
		threadArgs -> out = outP;
		threadArgs -> fOut = f_out;

		//Run a thread of compressVideo
		pthread_create(&threads[i], NULL, compressVideo, threadArgs);
		//Wait until the thread finishes
		pthread_join(threads[i], NULL);
		total_in += threadArgs -> in;
		total_out += threadArgs -> out;
	}

	fclose(f_out);

	//Print out the resulting compressed file size
	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// release list of files
	for(int i=0; i < nfiles; i++)
		free(files[i]);
	free(files);

	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer

	return 0;
}
