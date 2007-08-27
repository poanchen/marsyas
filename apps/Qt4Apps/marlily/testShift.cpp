#include "MarSystemManager.h"
#include <iostream>
using namespace Marsyas;
using namespace std;

int main(int argc, char *argv[])
{
	std::string infile = argv[1];

	MarSystemManager mng;
	MarSystem* pnet = mng.create("Series", "pnet");
    pnet->addMarSystem(mng.create("SoundFileSource", "src"));
    pnet->updctrl("SoundFileSource/src/mrs_string/filename", infile);
    pnet->linkControl("mrs_bool/notEmpty",
                     "SoundFileSource/src/mrs_bool/notEmpty");

    pnet->updctrl("SoundFileSource/src/mrs_natural/inSamples",128);
//    pnet->updctrl("SoundFileSource/src/mrs_natural/inSamples",2048);

//	pnet->addMarSystem(mng.create("ShiftInput", "shift"));
//	pnet->updctrl("ShiftInput/shift/mrs_natural/WindowSize",512);

	int i=0;
    while ( pnet->getctrl("mrs_bool/notEmpty")->toBool() )
	{
        pnet->tick();
		i++;
	}
	cout<<i<<endl;
	delete pnet;
}

