/*
* Include file defining the Channel class
*
* M Naylor 2020
*/
class Channel
{
public:
    enum ChannelStatus { BAD_CONFIG, ACTIVE, INACTIVE };
	enum eSignal { ONFALLING = FALLING, ONRISING = RISING, ONLOW = LOW, ONHIGH = HIGH, ONCHANGE = CHANGE };
	enum ePinMode { PIN_INPUT = INPUT, PIN_OUTPUT = OUTPUT, PIN_PULLUP = INPUT_PULLUP };
	Channel ( int iPin, ePinMode iPinMode, eSignal iSignalMode );
	~Channel ();
	ChannelStatus Status ();
	void Print ( String &sResult );
	bool StartISR ( void pISR(void) );
	bool StopISR ();
	ePinMode GetPinMode();
	eSignal GetSignal();
	virtual void ChannelISR();

  private:
    int           m_iPinMode;
    int           m_iPin;
    bool          m_bISRActive = false;
	int			  m_iSignalMode;
    ChannelStatus m_eChannelStatus = BAD_CONFIG;
};