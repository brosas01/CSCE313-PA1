/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Bella Rosas
	UIN: 834003921
	Date: 09/24/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m=MAX_MESSAGE;
	bool new_chan = false;
	vector<FIFORequestChannel*> channels;
	
	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		// Note: the colon after the letter indicated that the flag has a value associated with it
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi (optarg);
				break;
			case 'c':
				new_chan = true;
				break;
		}
	}

	// Task 1: Run the server as a child process
	// Make server (a child of client)
	// note: server needs './server', '-m', '<val for -m arg>', 'NULL'
		// NULL tells execvp that we're done giving arguments to the command
	pid_t pid = fork();
	if (pid == -1) {
		cerr << "fork failed \n";
        return 1;
	}
	if (pid == 0) {
		char* args[] = {
			(char*)"./server",
			(char*)"-m",
			(char*)to_string(m).c_str(),
			nullptr
		};
		execvp(args[0], args);
		perror("execvp failed"); // Only runs if execvp fails
		return 1;
	} 

    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	// Task 4: Creating a new channel
	FIFORequestChannel* new_channel = nullptr;
	if (new_chan) {
		// send newchannel request to the server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
    	cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
		// create a variable to hold the name of the new channel from the server
			// char* or string
		char* new_channel_name = new char[MAX_MESSAGE];
		// cread the response from the server
		cont_chan.cread(new_channel_name, sizeof(new_channel_name));
		// call the FIFORequestChannel constructor with the name from the server
			// dynamically create the channel -- call new with the constructor so that you can use it outside of the if statement
		new_channel = new FIFORequestChannel(new_channel_name, FIFORequestChannel::CLIENT_SIDE);
		// Push the new channel into the channels vector
		channels.push_back(new_channel);
		delete[] new_channel_name;
	}
	
	// Use last channel in vector to send all requests
	FIFORequestChannel* chan = channels.back();

	// Task 2: Requesting data points
	
	if (p!=-1 && t!=-1 && e!=-1) {
		// Task 2a: requesting a single data point
		// Run only if p, t, and e != -1
		char buf[MAX_MESSAGE]; // 256
		datamsg x(p, t, e);
		
		memcpy(buf, &x, sizeof(datamsg));
		chan->cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan->cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	} else if (p!=-1) {
		// Task 2b: Collecting 1000 points and putting in x1.csv file
		ofstream ofs("./received/x1.csv");
		char buf[MAX_MESSAGE]; // 256
		for (int i = 0; i < 1000; ++i) {
			double time = i * 0.004;
			datamsg d1(p, time, 1);
			datamsg d2(p, time, 2);

			double ecg1, ecg2;

			memcpy(buf, &d1, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg));
			chan->cread(&ecg1, sizeof(double));

			memcpy(buf, &d2, sizeof(datamsg));
			chan->cwrite(buf, sizeof(datamsg));
			chan->cread(&ecg2, sizeof(double));

			ofs << time << "," << ecg1 << "," << ecg2 << endl;
		}
		ofs.close();
	}
	
	// Task 3: Requesting files
	if (!filename.empty()) {
		// Getting file size
		filemsg fm(0, 0);
		string fname = filename;
		
		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan->cwrite(buf2, len);  // I want the file length;

		__int64_t filesize = 0;
		chan->cread(&filesize, sizeof(__int64_t));

		// Opening output file
		string outpath = "received/" + filename;
		FILE* fp = fopen(outpath.c_str(), "wb");
			// w = write mode, b = binary mode
		if (!fp) {
			cerr << "Failed to open output file: " << outpath << endl;
			delete[] buf2;
			return 1;
		}

		// Transfer in chunks
		__int64_t offset = 0;
		char* buf3 = new char[m];
		// loop over the segments in the filesize / buff capacity
		while (offset < filesize) {
			int chunk_size = min(m, (int)(filesize-offset));
			filemsg* file_req = (filemsg*)buf2;
			file_req->offset = offset;
			file_req->length = chunk_size;
			chan->cwrite(buf2, len);
			chan->cread(buf3, chunk_size);
			fwrite(buf3, 1, chunk_size, fp);
			offset += chunk_size;
		}
		// Cleanup
		fclose(fp);
		delete[] buf2;
		delete[] buf3;
	}
	
	
	// closing channels
	MESSAGE_TYPE qMessage = QUIT_MSG;
	if (new_chan) {
		new_channel->cwrite(&qMessage, sizeof(MESSAGE_TYPE));
		delete new_channel;
	}
	cont_chan.cwrite(&qMessage, sizeof(MESSAGE_TYPE));
	wait(nullptr);
}