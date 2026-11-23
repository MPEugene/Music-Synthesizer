#define UNICODE
#define _UNICODE
#include <iostream>
#include <windows.h>
#include <mmsystem.h>
using namespace std;
#include "olcNoiseMaker.h"

double w(double dHertz){
    return dHertz * 2.0 * PI;
}

double osc(double dHertz, double dTime, int nType){
    switch(nType){
        case 0: return sin(w(dHertz * dTime));  //sine wave
        case 1: return sin(w(dHertz) * dTime) > 0 ? 1.0 : -1.0; //square wave
        case 2: return asin(sin(w(dHertz) * dTime)) * (2.0 / PI);   //triangle wave
        case 3:{    //Saw wave (analogue / warm / slow)
            double dOutput = 0.0;

            for(double n = 1.0; n < 20.0; n++){
                dOutput += (sin(n * w(dHertz) * dTime)) / n;
            }
            return dOutput * (2.0 / PI);
        }

        case 4: return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0)); // Saw wave(optimised / harsh / fast )
        case 5: return 2.0 * ((double)rand() / (double)RAND_MAX) -1.0;  //pseudo random noise
        default: return 0;
    }
}

struct sEnvelopeADSR{
    double dAttackTime;
    double dDecayTime;
    double dReleaseTime;

    double dSustainAmplitude;
    double dStartAmplitude;

    double dTriggerOnTime;
    double dTriggerOffTime;

    bool bNoteOn;

    sEnvelopeADSR(){
        dAttackTime = 0.01;
        dDecayTime = 0.01;
        dStartAmplitude = 1.0;
        dSustainAmplitude = 0.8;
        dReleaseTime = 0.02;
        dTriggerOnTime = 0.0;
        dTriggerOffTime = 0.0;
        bNoteOn = false;
    }

    double GetAmplitude(double dTime){
        double dAmplitude = 0.0;
        double dLifeTime = dTime - dTriggerOnTime;
        if(bNoteOn){

            // ADS
            // Attack
            if(dLifeTime <= dAttackTime){
                dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;
            }

            // Decay
           if(dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime)){
                dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;
            }

            // Sustain
            if(dTime > (dAttackTime + dDecayTime)){
                dAmplitude = dSustainAmplitude;
            }
        } else{

            // Release
           dAmplitude = ((dTime - dTriggerOffTime) / dReleaseTime) * (0.0 - dSustainAmplitude) + dSustainAmplitude;
        }

        if(dAmplitude <= 0.0001){
            dAmplitude = 0;
        }


        return dAmplitude;

    }
    void NoteOn(double dTimeOn){
        dTriggerOnTime = dTimeOn;
        bNoteOn = true;
    }

    void NoteOff(double dTimeOff){
        dTriggerOffTime = dTimeOff;
        bNoteOn = false;
    }

};


// global synthesizer variables
atomic<double> dFrequencyOutput = 0.0; // dominant output frequency of instrument ... (e zoomed in video-ul)
double dOctaveBaseFrequency = 110.0; //A2
double d12thRootOf2 = pow(2.0, 1.0 / 12.0);
sEnvelopeADSR envelope;

double MakeNoise( double dTime ){

    double dOutput = envelope.GetAmplitude(dTime) * osc(dFrequencyOutput, dTime, 3);
    return dOutput * 0.4; // master volume
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
int nCurrentKey = -1;
while(1){
    
    bool keyPressed = false;
    for (int k = 0; k < 15; k++){

        if(GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe"[k])) & 0x8000){

            if(nCurrentKey != k){

                wcout << "\rNote On: " << sound.GetTime() << "s " << dFrequencyOutput;
                dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, k);
                envelope.NoteOn(sound.GetTime());
                nCurrentKey = k;
            }

            keyPressed = true;
        }
    }

    if(!keyPressed){

        if(nCurrentKey != -1){
            wcout << "\rNote Off: " << sound.GetTime() << "s";
            envelope.NoteOff(sound.GetTime());
            nCurrentKey = -1;
        }
    }
}
    return 0;
}