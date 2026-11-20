#define UNICODE
#define _UNICODE
#include <iostream>
#include <windows.h>
#include <mmsystem.h>
using namespace std;
#include "olcNoiseMaker.h"

double MakeNoise( double dTime ){

    return 0.5 * sin(440.0  * 2 * 3.14159 * dTime);
}

int main(){

//Get all sound hardware
    vector<wstring> devices = olcNoiseMaker<short>:: Enumerate();
 
    //Display findings
    for (auto d : devices) wcout << "Found Output Device: " << d << endl;

    //Create sound machine

if (devices.empty()) {
    wcout << L"No audio output devices found!" << endl;
    return 1;}
    
    olcNoiseMaker <short> sound(devices[0], 44100, 1 , 8, 512);

// Link noise function with sound machine

sound.SetUserFunction(MakeNoise);

while(1){}


    return 0;
}