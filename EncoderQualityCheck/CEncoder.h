/*
* Header file to define the 3 Channel rotary encoder class
* Author Mark Naylor 2020
*/
#include "CChannel.h"

class Encoder
{
public:
	enum eChannelId { ACHANNEL, BCHANNEL, ZCHANNEL };
	Encoder ( unsigned int uiNPR );
	~Encoder ();
	bool AddChannel ( eChannelId channelId, int iPinNum, Channel::ePinMode PinMode, Channel::eSignal Signal );
	unsigned long GetChannelZIntFrequencyMicros ();
	unsigned long GetChannelAIntFrequencyMicros ();
	unsigned long GetChannelBIntFrequencyMicros ();
	void GetState ( String &s );
	unsigned int GetNPR ();

private:
	void ResetZChannelData ();

	unsigned int	m_uiNPR 			= 0;
	Channel *		m_pAChannel			= nullptr;
	Channel *		m_pBChannel			= nullptr;
	Channel *		m_pZChannel			= nullptr;
	unsigned long 	m_ulMicrosRevTime	= 0L;

};