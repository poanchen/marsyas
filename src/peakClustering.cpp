// #include <vld.h>

#include <cstdio>
#include<iostream>
#include<iomanip>

#include "MarSystemManager.h"
#include "AudioSink.h"
#include "SoundFileSink.h"
#include "SoundFileSource.h"
#include "Gain.h"
#include "Messager.h"
#include "Conversions.h"
#include "CommandLineOptions.h"
#include "PeClusters.h"
#include "PeUtilities.h"
#include <time.h>

#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <string>

using namespace std;
using namespace Marsyas;

string pluginName = EMPTYSTRING;
string inputDirectoryName = EMPTYSTRING;
string outputDirectoryName = EMPTYSTRING;
string fileName = EMPTYSTRING;
string noiseName = EMPTYSTRING;
string fileResName = EMPTYSTRING;
string mixName = EMPTYSTRING;
string filePeakName = EMPTYSTRING;
string fileClustName = EMPTYSTRING;
string fileVoicingName = EMPTYSTRING;

// Global variables for command-line options 
bool helpopt_ = 0;
bool usageopt_ =0;
int fftSize_ = 2048;
int winSize_ = 2048;
// if kept the same no time expansion
int hopSize_ = 512;
// nb Sines
int nbSines_ = 20;
// nbClusters
int nbClusters_ = 3;
// nbClusters
int nbSelectedClusters_ = 0;
// output buffer Size
int bopt_ = 128;
// output gain
mrs_real gopt_ = 1.0;
// number of accumulated frames
mrs_natural accSize_ = 20;
// number of seconds for analysing process
mrs_natural stopAnalyse_=0;
// type of similarity Metrics // test amplitude normamlise gtzan
string defaultSimilarityType_ = "hofbab";
string similarityType_ = EMPTYSTRING;
// weight for similarity Metrics
realvec similarityWeight_;	
// store for clustered peaks
realvec peakSet_;
// delay for noise insertion
mrs_real noiseDelay_=0;
// gain for noise insertion
mrs_real noiseGain_=.8;
// duration for noise insertion
mrs_real noiseDuration_=0;
// sampling frequency
mrs_real samplingFrequency_=1;
//
mrs_real timeElapsed;
//
mrs_natural nbTicks=0;

bool microphone_ = false;
bool analyse_ = false;
bool attributes_ = false;
bool ground_ = false;
bool synthetize_ = false;
bool clusterSynthetize_ = false;
bool peakStore_= false;

CommandLineOptions cmd_options;

void 
printUsage(string progName)
{
	MRSDIAG("peakClustering.cpp - printUsage");
	cerr << "Usage : " << progName << " [file]" << endl;
	cerr << endl;
	cerr << "If no filename is given the default live audio input is used. " << endl;
}

void 
printHelp(string progName)
{
	MRSDIAG("peakClustering.cpp - printHelp");
	cerr << "peakClustering, MARSYAS, Copyright Mathieu Lagrange " << endl;
	cerr << "--------------------------------------------" << endl;
	cerr << "Usage : " << progName << " [file]" << endl;
	cerr << endl;
	cerr << "if no filename is given the default live audio input is used. " << endl;
	cerr << "Options:" << endl;
	cerr << "-n --fftsize         : size of fft " << endl;
	cerr << "-w --winsize         : size of window " << endl;
	cerr << "-s --sinusoids       : number of sinusoids" << endl;
	cerr << "-b --buffersize      : audio buffer size" << endl;
	cerr << "-g --gain            : gain (0.0-1.0) " << endl;
	cerr << "-f --filename        : output filename" << endl;
	cerr << "-o --outputdirectorypath   : output directory path" << endl;
	cerr << "-i --inputdirectorypath   : input directory path" << endl;
	cerr << "-u --usage           : display short usage info" << endl;
	cerr << "-h --help            : display this information " << endl;

	exit(1);
}


// original monophonic peakClustering 



void
clusterExtract(realvec &peakSet, string sfName, string outsfname, string noiseName, string mixName, mrs_real noiseDelay, string T, mrs_natural N, mrs_natural Nw, 
							 mrs_natural D, mrs_natural S, mrs_natural C,
							 mrs_natural accSize, bool synthetize)
{
	cout << "Extracting Peaks and Clusters" << endl;
	MarSystemManager mng;

	// create the phasevocoder network
	MarSystem* pvseries = mng.create("Series", "pvseries");

	//create accumulator series
	MarSystem* preNet = mng.create("Series", "preNet");
	//create fanout for mixing
	MarSystem* fanin = mng.create("Fanin", "fanin");
	// add original source in the fanout
	if (microphone_) 
		fanin->addMarSystem(mng.create("AudioSource", "src"));
	else 
		fanin->addMarSystem(mng.create("SoundFileSource", "src"));
	// create a series for the noiseSource
	MarSystem* mixseries = mng.create("Series", "mixseries");
	
	if(noiseName == "white")
mixseries->addMarSystem(mng.create("NoiseSource", "noise"));
	else
	mixseries->addMarSystem(mng.create("SoundFileSource", "noise"));

	mixseries->addMarSystem(mng.create("Delay", "noiseDelay"));
	MarSystem* noiseGain = mng.create("Gain", "noiseGain");
	mixseries->addMarSystem(noiseGain);
	// add this series in the fanout
	fanin->addMarSystem(mixseries);

	preNet->addMarSystem(fanin);
	//////////////////////////////////////////////////////
  // should be removed for weird command line problems
  preNet->addMarSystem(mng.create("SoundFileSink", "mixSink"));
	////////////////////////////////////////
	preNet->addMarSystem(mng.create("ShiftInput", "si"));
	preNet->addMarSystem(mng.create("Shifter", "sh"));
	preNet->addMarSystem(mng.create("Windowing", "wi"));

	MarSystem *parallel = mng.create("Parallel", "par");
	parallel->addMarSystem(mng.create("Spectrum", "spk1"));
	parallel->addMarSystem(mng.create("Spectrum", "spk2"));
	preNet->addMarSystem(parallel);

	preNet->addMarSystem(mng.create("PeConvert", "conv"));
	//create accumulator
	MarSystem* accumNet = mng.create("Accumulator", "accumNet");
	accumNet->addMarSystem(preNet);

	/*************************************************************/

	MarSystem* peClust = mng.create("PeClust", "peClust");

	/*************************************************************/

	//create the main network
	pvseries->addMarSystem(accumNet);
	pvseries->addMarSystem(peClust);

	/*************************************************************/

	if(synthetize) 
	{
		//create shredder
		synthNetCreate(&mng, outsfname, microphone_);
		MarSystem *peSynth = mng.create("PeSynthetize", "synthNet");
		pvseries->addMarSystem(peSynth);
	}

	////////////////////////////////////////////////////////////////
	// update the controls
	////////////////////////////////////////////////////////////////
	pvseries->updctrl("Accumulator/accumNet/mrs_natural/nTimes", accSize);

	if (microphone_) 
	{
		pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/AudioSource/src/mrs_natural/inSamples", D);
		pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/AudioSource/src/mrs_natural/inObservations", 1);
	}
	else
	{
		pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_string/filename", sfName);
		pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_natural/inSamples", D);
		//pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_natural/inObservations", 1);
		samplingFrequency_ = pvseries->getctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_real/osrate")->toReal();
	}


	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/Series/mixseries/SoundFileSource/noise/mrs_string/filename", noiseName);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/Series/mixseries/SoundFileSource/noise/mrs_natural/inSamples", D);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/Series/mixseries/NoiseSource/noise/mrs_string/mode", "rand");
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/Series/mixseries/Delay/noiseDelay/mrs_real/delay",  noiseDelay);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/Series/mixseries/Gain/noiseGain/mrs_real/gain", noiseGain_);

	pvseries->updctrl("Accumulator/accumNet/Series/preNet/ShiftInput/si/mrs_natural/Decimation", D);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/ShiftInput/si/mrs_natural/WindowSize", Nw+1);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Windowing/wi/mrs_natural/size", N);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Windowing/wi/mrs_string/type", "Hanning");
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Windowing/wi/mrs_natural/zeroPhasing", 1);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Shifter/sh/mrs_natural/shift", 1);
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/PvFold/fo/mrs_natural/Decimation", D); // useless ?
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/PeConvert/conv/mrs_natural/Decimation", D);      
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/PeConvert/conv/mrs_natural/Sinusoids", S);  
  // pvseries->updctrl("Accumulator/accumNet/Series/preNet/PeConvert/conv/mrs_natural/nbFramesSkipped", (N/D));  

	pvseries->setctrl("PeClust/peClust/mrs_natural/Sinusoids", S);  
	pvseries->setctrl("PeClust/peClust/mrs_natural/Clusters", C); 
	pvseries->setctrl("PeClust/peClust/mrs_natural/selectedClusters", nbSelectedClusters_); 
	pvseries->setctrl("PeClust/peClust/mrs_natural/hopSize", D); 
	pvseries->setctrl("PeClust/peClust/mrs_natural/storePeaks", (mrs_natural) peakStore_); 
	pvseries->updctrl("PeClust/peClust/mrs_string/similarityType", T); 

	similarityWeight_.stretch(3);
  similarityWeight_(0) = 1;
  similarityWeight_(1) = 10;
  similarityWeight_(2) = 1;

	pvseries->updctrl("PeClust/peClust/mrs_realvec/similarityWeight", similarityWeight_); 

	pvseries->updctrl("Accumulator/accumNet/Series/preNet/SoundFileSink/mixSink/mrs_string/filename", mixName);//[!]

	//pvseries->update();
							
	if(synthetize)
	{
		 synthNetConfigure (pvseries, sfName, outsfname, fileResName, Nw, D, S, accSize, microphone_, bopt_, Nw+1-D);
	}

	if(noiseDuration_)
	{
	ostringstream ossi;
	ossi << ((noiseDelay_+noiseDuration_)) << "s";
	cout << ossi.str() << endl;
	// touch the gain directly
//	noiseGain->updctrl("0.1s", Repeat("0.1s", 1), new EvValUpd(noiseGain,"mrs_real/gain", 0.0));

	}

//	cout << *pvseries << endl;
	
	mrs_real globalSnr = 0;
	mrs_natural nb=0;
	//	mrs_real time=0;
	while(1)
	{	
//		cout << "tick"<<endl;
		pvseries->tick();
//	cout << "tick"<<endl;
	/*	if(time > (noiseDelay_+noiseDuration_))
		{
	pvseries->updctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/Series/mixseries/Gain/noiseGain/mrs_real/gain", (mrs_real) 0);
		}
    time+=accSize_*hopSize_/samplingFrequency_;*/
// cout << time << " " << noiseDelay_+noiseDuration_ << endl;
		// ouput the seg snr
		if(synthetize)
		{
			mrs_real snr = pvseries->getctrl("PeSynthetize/synthNet/Series/postNet/PeResidual/res/mrs_real/snr")->toReal();
			globalSnr+=snr;
			nb++;
			 // cout << "Frame " << nb << " SNR : "<< snr << endl;
		}

		if (!microphone_)
		{
			bool temp = pvseries->getctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_bool/notEmpty")->toBool();
			bool temp1 = accumNet->getctrl("Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_bool/notEmpty")->toBool();
			bool temp2 = preNet->getctrl("Fanin/fanin/SoundFileSource/src/mrs_bool/notEmpty")->toBool();
			
			mrs_real timeRead =  preNet->getctrl("Fanin/fanin/SoundFileSource/src/mrs_natural/pos")->toNatural()/samplingFrequency_;
			mrs_real timeLeft;
			if(!stopAnalyse_)
				timeLeft =  preNet->getctrl("Fanin/fanin/SoundFileSource/src/mrs_natural/size")->toNatural()/samplingFrequency_;
			else
				timeLeft = stopAnalyse_;
			// string fname = pvseries->getctrl("Accumulator/accumNet/Series/preNet/Fanin/fanin/SoundFileSource/src/mrs_string/filename")->toString();
			printf("%.2f / %.2f \r", timeRead, timeLeft);
			//cout << fixed << setprecision(2) << timeRead << "/" <<  setprecision(2) << timeLeft;
			///*bool*/ temp = pvseries->getctrl("Accumulator/accumNet/Series/preNet/SoundFileSource/src/mrs_bool/notEmpty")->toBool();
			if (temp2 == false || (stopAnalyse_ !=0 && stopAnalyse_<timeRead))
				break;
		}
	}
	if(synthetize_)
		cout << "Global SNR : " << globalSnr/nb << endl;

	// plot and save peak data
	peakSet = pvseries->getctrl("PeClust/peClust/mrs_realvec/peakSet")->toVec();


	ofstream peakFile;
	peakFile.open(filePeakName.c_str());
	if(!peakFile)
		cout << "Unable to open output Peaks File " << filePeakName << endl;
	peakFile << peakSet_;
	peakFile.close();
}



void 
initOptions()
{
	cmd_options.addBoolOption("help", "h", false);
	cmd_options.addBoolOption("usage", "u", false);
	cmd_options.addNaturalOption("voices", "v", 1);
	cmd_options.addStringOption("filename", "f", EMPTYSTRING);
	cmd_options.addStringOption("noisename", "N", EMPTYSTRING);
	cmd_options.addStringOption("outputdirectoryname", "o", EMPTYSTRING);
	cmd_options.addStringOption("inputdirectoryname", "i", EMPTYSTRING);
	cmd_options.addStringOption("typeSimilarity", "t", defaultSimilarityType_);
	cmd_options.addNaturalOption("winsize", "w", winSize_);
	cmd_options.addNaturalOption("fftsize", "n", fftSize_);
	cmd_options.addNaturalOption("sinusoids", "s", nbSines_);
	cmd_options.addNaturalOption("bufferSize", "b", bopt_);
  cmd_options.addNaturalOption("quitAnalyse", "q", stopAnalyse_);
	cmd_options.addNaturalOption("clustering", "c", nbClusters_);
	cmd_options.addNaturalOption("pruning", "p", nbSelectedClusters_);

	cmd_options.addBoolOption("analyse", "a", analyse_);
	cmd_options.addBoolOption("attributes", "A", attributes_);
	cmd_options.addBoolOption("ground", "g", ground_);
	cmd_options.addBoolOption("synthetize", "s", synthetize_);
	cmd_options.addBoolOption("clusterSynthetize", "S", clusterSynthetize_);
  cmd_options.addBoolOption("peakStore", "P", peakStore_);
}


void 
loadOptions()
{
	helpopt_ = cmd_options.getBoolOption("help");
	usageopt_ = cmd_options.getBoolOption("usage");
	pluginName = cmd_options.getStringOption("plugin");
	fileName   = cmd_options.getStringOption("filename");
	inputDirectoryName = cmd_options.getStringOption("inputdirectoryname");
	outputDirectoryName = cmd_options.getStringOption("outputdirectoryname");
	similarityType_ = cmd_options.getStringOption("typeSimilarity");
	noiseName = cmd_options.getStringOption("noisename");
	winSize_ = cmd_options.getNaturalOption("winsize");
	fftSize_ = cmd_options.getNaturalOption("fftsize");
	nbSines_ = cmd_options.getNaturalOption("sinusoids");
	bopt_ = cmd_options.getNaturalOption("bufferSize");
	stopAnalyse_ = cmd_options.getNaturalOption("quitAnalyse");
  nbClusters_ = cmd_options.getNaturalOption("clustering");
  nbSelectedClusters_ = cmd_options.getNaturalOption("pruning");

	analyse_ = cmd_options.getBoolOption("analyse");
	attributes_ = cmd_options.getBoolOption("attributes");
	ground_ = cmd_options.getBoolOption("ground");
	synthetize_ = cmd_options.getBoolOption("synthetize");
	clusterSynthetize_ = cmd_options.getBoolOption("clusterSynthetize");
  peakStore_ = cmd_options.getBoolOption("peakStore"); 
}



int
main(int argc, const char **argv)
{
	MRSDIAG("sftransform.cpp - main");

	initOptions();
	cmd_options.readOptions(argc, argv);
	loadOptions();  

	vector<string> soundfiles = cmd_options.getRemaining();
	vector<string>::iterator sfi;


	string progName = argv[0];  


	if (helpopt_) 
		printHelp(progName);

	if (usageopt_)
		printUsage(progName);


	cerr << "peakClustering configuration (-h show the options): " << endl;
	cerr << "fft size (-n)      = " << fftSize_ << endl;
	cerr << "win size (-w)      = " << winSize_ << endl;
	cerr << "sinusoids (-s)     = " << nbSines_ << endl;
	cerr << "outFile  (-f)      = " << fileName << endl;
	cerr << "outputDirectory  (-o) = " << outputDirectoryName << endl;
	cerr << "inputDirectory  (-i) = " << inputDirectoryName << endl;

	nbTicks = clock();
	// extract peaks and clusters
	// soundfile input 
	string sfname;
	if (soundfiles.size() != 0)   
	{
		// process several soundFiles
		for (sfi=soundfiles.begin() ; sfi!=soundfiles.end() ; sfi++)
		{
			FileName Sfname(*sfi);
			if(outputDirectoryName != EMPTYSTRING)
			{

				fileName = outputDirectoryName + "/" + Sfname.name() ;
				fileResName = outputDirectoryName + "/" + Sfname.nameNoExt() + "Res." + Sfname.ext() ;
				mixName = outputDirectoryName + "/" + Sfname.nameNoExt() + "Mix." + Sfname.ext() ;
				filePeakName = outputDirectoryName + "/" + Sfname.nameNoExt() + "Peak.txt" ;
				fileClustName = outputDirectoryName + "/" + Sfname.nameNoExt() + "Clust.txt" ;
        fileVoicingName = outputDirectoryName + "/" + Sfname.nameNoExt() + "Voicing.txt" ;

				if(noiseName == "music")
				{
					string tmp = Sfname.nameNoExt();
					tmp.replace(tmp.length()-1, 1, 1, 'M');
					noiseName = Sfname.path() +tmp + "." +  Sfname.ext();
				}
				cout << noiseName << endl;
			}
			if(analyse_)
			{
				cout << "Phasevocoding " << Sfname.name() << endl; 
				clusterExtract(peakSet_, *sfi, fileName, noiseName, mixName, noiseDelay_, similarityType_, fftSize_, winSize_, hopSize_, nbSines_, nbClusters_, accSize_, synthetize_);
			}	
			// if ! peak data read from file
			if(peakSet_.getSize() == 0)
				peakSet_.read(filePeakName);
			if(peakSet_.getSize() == 0)
			{
				cout << "unable to load peak file: " << filePeakName << endl;
				exit(1);
			}

			MATLAB_PUT(peakSet_, "peaks");
			MATLAB_EVAL("figure(1); clf ; plotPeaks(peaks, -1)");


			// create data for clusters
			PeClusters clusters(peakSet_);
			mrs_natural nbClusters=0;
			// computes the cluster attributes
			if(attributes_)
			{
				realvec vecs;
				clusters.attributes(peakSet_);	
				clusters.getVecs(vecs);
	
				// cout << vecs;
				MATLAB_PUT(getcwd(NULL, 0), "path");
			  MATLAB_PUT(fileName, "fileName");
				
        MATLAB_PUT(vecs, "clusters");
				ofstream clustFile;
				clustFile.open(fileClustName.c_str());
				if(!clustFile)
					cout << "Unable to open output Clusters File " << fileClustName << endl;
			//	clustFile << vecs;
				clustFile.close();

				// store voicingLine
				clusters.voicingLine(fileVoicingName, hopSize_);
			}
			
			// compute ground truth 
			if(ground_)
			{
				realvec vecs;
				clusters.synthetize(peakSet_, *sfi, fileName, winSize_, hopSize_, nbSines_, bopt_, 1);
				clusters.getVecs(vecs);
				MATLAB_PUT(vecs, "clusters");
				clusters.selectGround();
				realvec ct;
				
				

				clusters.getConversionTable(ct);
				updateLabels(peakSet_, ct);
			//	cout << ct;
			}
			if(ground_ || attributes_)
				MATLAB_EVAL("figure(2) ; plotClusters");
			/*	MATLAB_PUT(peakSet_, "peaksGp");
			MATLAB_EVAL("plotPeaks(peaksGp)");*/

			if(clusterSynthetize_)
			{
				PeClusters sclusters(peakSet_);
				// synthetize remaining clusters
				sclusters.synthetize(peakSet_, *sfi, fileName, winSize_, hopSize_, nbSines_, bopt_);
			}
			/*MATLAB_PUT(peakSet_, "peaks");
			MATLAB_EVAL("plotPeaks(peaks)");*/
		}
	}
	else
	{
		cout << "Using live microphone input" << endl;
		microphone_ = true;
		clusterExtract(peakSet_, "microphone", fileName, noiseName, mixName, noiseDelay_, similarityType_, fftSize_, winSize_, hopSize_, nbSines_, nbClusters_, accSize_, synthetize_);
	}

 timeElapsed = (clock()-nbTicks)/((mrs_real) CLOCKS_PER_SEC );
 cout << "Time elapsed: " << timeElapsed << endl;
	
 exit(0);
}


