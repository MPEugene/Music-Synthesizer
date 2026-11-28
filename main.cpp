#define UNICODE
#define _UNICODE
#include <iostream>
#include <windows.h>
#include <mmsystem.h>
#include <list>
using namespace std;
#include "olcNoiseMaker.h"
#define FTYPE double


namespace synth
{
	
	// Utilities

	// Converts frequency (Hz) to angular velocity
	FTYPE w(const FTYPE dHertz)
	{
		return dHertz * 2.0 * PI;
	}

	// A basic note
	struct note
	{
		int id;		// Position in scale
		FTYPE on;	// Time note was activated
		FTYPE off;	// Time note was deactivated
		bool active;
		instrument_base *channel;

		note()
		{
			id = 0;
			on = 0.0;
			off = 0.0;
			active = false;
			channel = 0;
		}
	};

	
	// Multi-Function Oscillator
	const int OSC_SINE = 0;
	const int OSC_SQUARE = 1;
	const int OSC_TRIANGLE = 2;
	const int OSC_SAW_ANA = 3;
	const int OSC_SAW_DIG = 4;
	const int OSC_NOISE = 5;

	FTYPE osc(const FTYPE dTime, const FTYPE dHertz, const int nType = OSC_SINE,
		const FTYPE dLFOHertz = 0.0, const FTYPE dLFOAmplitude = 0.0, FTYPE dCustom = 50.0)
	{

		FTYPE dFreq = w(dHertz) * dTime + dLFOAmplitude * dHertz * (sin(w(dLFOHertz) * dTime));// osc(dTime, dLFOHertz, OSC_SINE);

		switch (nType)
		{
		case OSC_SINE: // Sine wave bewteen -1 and +1
			return sin(dFreq);

		case OSC_SQUARE: // Square wave between -1 and +1
			return sin(dFreq) > 0 ? 1.0 : -1.0;

		case OSC_TRIANGLE: // Triangle wave between -1 and +1
			return asin(sin(dFreq)) * (2.0 / PI);

		case OSC_SAW_ANA: // Saw wave (analogue / warm / slow)
		{
			FTYPE dOutput = 0.0;
			for (FTYPE n = 1.0; n < dCustom; n++)
				dOutput += (sin(n*dFreq)) / n;
			return dOutput * (2.0 / PI);
		}

		case OSC_SAW_DIG:
			return (2.0 / PI) * (dHertz * PI * fmod(dTime, 1.0 / dHertz) - (PI / 2.0));

		case OSC_NOISE:
			return 2.0 * ((FTYPE)rand() / (FTYPE)RAND_MAX) - 1.0;

		default:
			return 0.0;
		}
	}

	
	// Scale to Frequency conversion

	const int SCALE_DEFAULT = 0;

	FTYPE scale(const int nNoteID, const int nScaleID = SCALE_DEFAULT)
	{
		switch (nScaleID)
		{
		case SCALE_DEFAULT: default:
			return 256 * pow(1.0594630943592952645618252949463, nNoteID);
		}		
	}


	
	// Envelopes

	struct envelope
	{
		virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff) = 0;
	};

	struct envelope_adsr : public envelope
	{
		FTYPE dAttackTime;
		FTYPE dDecayTime;
		FTYPE dSustainAmplitude;
		FTYPE dReleaseTime;
		FTYPE dStartAmplitude;

		envelope_adsr()
		{
			dAttackTime = 0.1;
			dDecayTime = 0.1;
			dSustainAmplitude = 1.0;
			dReleaseTime = 0.2;
			dStartAmplitude = 1.0;
		}

		virtual FTYPE amplitude(const FTYPE dTime, const FTYPE dTimeOn, const FTYPE dTimeOff)
		{
			FTYPE dAmplitude = 0.0;
			FTYPE dReleaseAmplitude = 0.0;

			if (dTimeOn > dTimeOff) // Note is on
			{
				FTYPE dLifeTime = dTime - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dAmplitude = dSustainAmplitude;
			}
			else // Note is off
			{
				FTYPE dLifeTime = dTimeOff - dTimeOn;

				if (dLifeTime <= dAttackTime)
					dReleaseAmplitude = (dLifeTime / dAttackTime) * dStartAmplitude;

				if (dLifeTime > dAttackTime && dLifeTime <= (dAttackTime + dDecayTime))
					dReleaseAmplitude = ((dLifeTime - dAttackTime) / dDecayTime) * (dSustainAmplitude - dStartAmplitude) + dStartAmplitude;

				if (dLifeTime > (dAttackTime + dDecayTime))
					dReleaseAmplitude = dSustainAmplitude;

				dAmplitude = ((dTime - dTimeOff) / dReleaseTime) * (0.0 - dReleaseAmplitude) + dReleaseAmplitude;
			}

			// Amplitude should not be negative
			if (dAmplitude <= 0.000)
				dAmplitude = 0.0;

			return dAmplitude;
		}
	};

	FTYPE env(const FTYPE dTime, envelope &env, const FTYPE dTimeOn, const FTYPE dTimeOff)
	{
		return env.amplitude(dTime, dTimeOn, dTimeOff);
	}


	struct instrument_base
	{
		FTYPE dVolume;
		synth::envelope_adsr env;
		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished) = 0;
	};

	struct instrument_bell : public instrument_base
	{
		instrument_bell()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 1.0;

			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 1.00 * synth::osc(n.on - dTime, synth::scale(n.id + 12), synth::OSC_SINE, 5.0, 0.001)
				+ 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 24))
				+ 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + 36));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_bell8 : public instrument_base
	{
		instrument_bell8()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.5;
			env.dSustainAmplitude = 0.8;
			env.dReleaseTime = 1.0;

			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+1.00 * synth::osc(n.on - dTime, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 12))
				+ 0.25 * synth::osc(n.on - dTime, synth::scale(n.id + 24));

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_harmonica : public instrument_base
	{
		instrument_harmonica()
		{
			env.dAttackTime = 0.05;
			env.dDecayTime = 1.0;
			env.dSustainAmplitude = 0.95;
			env.dReleaseTime = 0.1;

			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				//+ 1.0  * synth::osc(n.on - dTime, synth::scale(n.id-12), synth::OSC_SAW_ANA, 5.0, 0.001, 100)
				+ 1.00 * synth::osc(n.on - dTime, synth::scale(n.id), synth::OSC_SQUARE, 5.0, 0.001)
				+ 0.50 * synth::osc(n.on - dTime, synth::scale(n.id + 12), synth::OSC_SQUARE)
				+ 0.05  * synth::osc(n.on - dTime, synth::scale(n.id + 24), synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}

	};

	struct instrument_drumkick : public instrument_base
	{
		instrument_drumkick()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.15;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 0.99 * synth::osc(dTime - n.on, synth::scale(n.id - 36), synth::OSC_SINE, 1.0, 1.0)
				+ 0.01 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}
	};

	struct instrument_drumsnare : public instrument_base
	{
		instrument_drumsnare()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.2;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound =
				+ 0.5 * synth::osc(dTime - n.on, synth::scale(n.id - 24), synth::OSC_SINE, 0.5, 1.0)
				+ 0.5 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}
	};

	struct instrument_drumhihat : public instrument_base
	{
		instrument_drumhihat()
		{
			env.dAttackTime = 0.01;
			env.dDecayTime = 0.05;
			env.dSustainAmplitude = 0.0;
			env.dReleaseTime = 0.0;
			dVolume = 1.0;
		}

		virtual FTYPE sound(const FTYPE dTime, synth::note n, bool &bNoteFinished)
		{
			FTYPE dAmplitude = synth::env(dTime, env, n.on, n.off);
			if (dAmplitude <= 0.0) bNoteFinished = true;

			FTYPE dSound = 0.1 * synth::osc(dTime - n.on, 0, synth::OSC_NOISE);

			return dAmplitude * dSound * dVolume;
		}
	};

	
}

vector<synth::note> vecNotes;
mutex muxNotes;
synth::instrument_bell instBell;
synth::instrument_bell8 instBell8;
synth::instrument_harmonica instHarm;
synth::instrument_drumkick instKick;
synth::instrument_drumsnare instSnare;
synth::instrument_drumhihat instHiHat;

int nCurrentInstrument = 1;
bool bBeatPlaying = false;


typedef bool(*lambda)(synth::note const& item);
template<class T>
void safe_remove(T &v, lambda f)
{
	auto n = v.begin();
	while (n != v.end())
		if (!f(*n))
			n = v.erase(n);
		else
			++n;
}

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
FTYPE MakeNoise(int channel, FTYPE dTime)
{	
	unique_lock<mutex> lm(muxNotes);
	FTYPE dMixedOutput = 0.0;

	for (auto &n : vecNotes)
	{
		bool bNoteFinished = false;
		FTYPE dSound = 0;
		
		if(n.channel != nullptr){
			dSound = n.channel->sound(dTime, n, bNoteFinished);
		}

		dMixedOutput += dSound;

		if(bNoteFinished){
			n.active = false;
		}
	}

	// Woah! Modern C++ Overload!!!
	safe_remove<vector<synth::note>>(vecNotes, [](synth::note const& item) { return item.active; });


	return dMixedOutput * 0.2;
}

int main()
{
	
	

	// Get all sound hardware
	vector<wstring> devices = olcNoiseMaker<short>::Enumerate();

	// Display findings
	for (auto d : devices) wcout << "Found Output Device: " << d << endl;
    if (devices.empty()) {
    wcout << L"No audio output devices found!" << endl;
    return 1;
}
	wcout << "Using Device: " << devices[0] << endl;

	// Display a keyboard
	wcout << endl <<
		"|   |   |   |   |   | |   |   |   |   | |   | |   |   |   |" << endl <<
		"|   | S |   |   | F | | G |   |   | J | | K | | L |   |   |" << endl <<
		"|   |___|   |   |___| |___|   |   |___| |___| |___|   |   |__" << endl <<
		"|     |     |     |     |     |     |     |     |     |     |" << endl <<
		"|  Z  |  X  |  C  |  V  |  B  |  N  |  M  |  ,  |  .  |  /  |" << endl <<
		"|_____|_____|_____|_____|_____|_____|_____|_____|_____|_____|" << endl << endl;


	// Create sound machine!!
	olcNoiseMaker<short> sound(devices[0], 44100, 1, 8, 256);

	// Link noise function with sound machine
	sound.SetUserFunction(MakeNoise);

	char keyboard[129];
	memset(keyboard, ' ', 127);
	keyboard[128] = '\0';

	auto clock_old_time = chrono::high_resolution_clock::now();
	auto clock_real_time = chrono::high_resolution_clock::now();
	double dElapsedTime = 0.0;

	double dBeatTimer = 0.0;
	double dLastTime = sound.GetTime();
	int nBeatStep = 0;
	bool bSpaceWasDown = false;

	DrawPiano(vecNotes);

	while (1)
	{

		// 1. Switch melody instrument with keys 1, 2, 3
		if ((GetAsyncKeyState('1') & 0x8000) && nCurrentInstrument != 1) { nCurrentInstrument = 1; bStateChanged = true; }
		if ((GetAsyncKeyState('2') & 0x8000) && nCurrentInstrument != 2) { nCurrentInstrument = 2; bStateChanged = true; }
		if ((GetAsyncKeyState('3') & 0x8000) && nCurrentInstrument != 3) { nCurrentInstrument = 3; bStateChanged = true; }

		// 2. Toggle background drum beat with SPACEBAR
		bool bSpaceDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
		if (bSpaceDown && !bSpaceWasDown)
		{
			bBeatPlaying = !bBeatPlaying;
			nBeatStep = 0;
			dBeatTimer = 0.0;
			bStateChanged = true;
		}
		bSpaceWasDown = bSpaceDown;

		// 3. Automated 16-step drum pattern (Part 4 Sequencer logic)
		double dTimeNow = sound.GetTime();
		double dElapsed = dTimeNow - dLastTime;
		dLastTime = dTimeNow;

		if (bBeatPlaying)
		{
			dBeatTimer += dElapsed;
			if (dBeatTimer >= 0.125) // 120 BPM 16th-note step
			{
				dBeatTimer -= 0.125;

				// 16-step patterns: 'X' = trigger, '.' = rest
				const char* patKick  = "X...X...X...X...";
				const char* patSnare = "....X.......X...";
				const char* patHiHat = "X.X.X.X.X.X.X.XX";

				muxNotes.lock();
				auto triggerDrum = [&](int channel) {
					synth::note dn;
					dn.id = 0;
					dn.on = dTimeNow;
					dn.off = dTimeNow + 0.01; // Percussive auto-release
					dn.channel = channel;
					dn.active = true;
					vecNotes.emplace_back(dn);
				};

				if (patKick[nBeatStep] == 'X')  triggerDrum(4);
				if (patSnare[nBeatStep] == 'X') triggerDrum(5);
				if (patHiHat[nBeatStep] == 'X') triggerDrum(6);
				muxNotes.unlock();

				nBeatStep = (nBeatStep + 1) % 16;
			}
		}

		for (int k = 0; k < 16; k++)
		{
			short nKeyState = GetAsyncKeyState((unsigned char)("ZSXCFVGBNJMK\xbcL\xbe\xbf"[k]));
			
			double dTimeNow = sound.GetTime();

			// Check if note already exists in currently playing notes
			muxNotes.lock();
			auto noteFound = find_if(vecNotes.begin(), vecNotes.end(), [&k](synth::note const& item) { return item.id == k; });
			if (noteFound == vecNotes.end())
			{
				// Note not found in vector

				if (nKeyState & 0x8000)
				{
					// Key has been pressed so create a new note
					synth::note n;
					n.id = k;
					n.on = dTimeNow;
					n.channel = 1;
					n.active = true;

					// Add note to vector
					vecNotes.emplace_back(n);
				}
				else
				{
					// Note not in vector, but key has been released...
					// ...nothing to do
				}
			}
			else
			{
				// Note exists in vector
				if (nKeyState & 0x8000)
				{
					// Key is still held, so do nothing
					if (noteFound->off > noteFound->on)
					{
						// Key has been pressed again during release phase
						noteFound->on = dTimeNow;
						noteFound->active = true;
					}
				}
				else
				{
					// Key has been released, so switch off
					if (noteFound->off < noteFound->on)
					{
						noteFound->off = dTimeNow;
					}
				}
			}
			muxNotes.unlock();		
		}
		wcout << "\rNotes: " << vecNotes.size() << "    ";
		const wchar_t* instName = (nCurrentInstrument == 1) ? L"Harmonica" :
	                          (nCurrentInstrument == 2) ? L"Bell" : L"8-Bit Bell";

		wcout << L"Instrument [1-3]: " << instName << L"   |   Drum Beat [SPACE]: "
			<< (bBeatPlaying ? L"ON " : L"OFF") << L"   " << endl;

			//this_thread::sleep_for(5ms);
	}

	return 0;
}