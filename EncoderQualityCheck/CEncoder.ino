/*
* Class file to encapsulate a 3 Channel rotary encoder
* Author Mark Naylor 2020
*/
namespace
{
#define	NUM_STABLE_REVS			20												// Number of revs with stable intervals that are required to meet criteria to continue
#define REV_TOLERANCE_FROM_AVG	5												// All stable revs must be within this percentage of average of set of candidate revs
#define ACCEPTABLE_MARGIN		10												// Actual timings can be +/- this value from expected
}
namespace ZChannelData
{
			//  unsigned long	ulLastRevTime   	= 0L;
			 unsigned long	ulZMargin			= 0L;							// This is REV_TOLERANCE_FROM_AVG percent of the expected rev time in microseconds

	volatile unsigned long	ulAvgZInterval		= 0L;							// This is the average of the last NUM_STABLE_REVS revs and when stable this becomes the expected time for a rev in microseconds
	volatile bool			bRevRateDetermined 	= false;						// set to true when revs are settled down
	volatile bool			bZStable			= true;
	volatile unsigned long  ulZUnstableRevTime	= 0UL;							// microseconds that were taken in revolution that exceeded expected tolerance
	volatile bool			bZResultProcessed	= true;							// Bool used to determine if non interrupt code has processed Z channel error info
	volatile long			lZRevDeviation		= 0L;							// percent deviation from expected timings * 100
}
namespace ABChannelData
{
			unsigned int	uiNPR				= 0;							// Pulses per revolution 
			unsigned long	ulABInterval		= 0L;							// This is the expected time between signals on the A or B channels in microseconds
			unsigned long	ulABMargin			= 0L;							// This is REV_TOLERANCE_FROM_AVG percent of the A or B signal interval in microseconds
			unsigned long	ulAChannelCount		= 0L;
			unsigned long	ulBChannelCount		= 0L;
			unsigned long	ulAChannelInts		= 0L;							// Number of A signals during last rev
			unsigned long	ulBChannelInts		= 0L;							// Number of B signals during last rev

}
static void ZISR ( void )
{
	static unsigned long ulTimeofLastPulse = 0L;
		   unsigned long ulTimeofPulse = micros();
	if ( ulTimeofLastPulse != 0 )
	{
		// calculate time between pulses
		unsigned long ulTimeBetweenPulses = ulTimeofPulse - ulTimeofLastPulse;
		// calculate RPM
		unsigned int uiZRPM = ( 60L * 1000000L ) / ulTimeBetweenPulses;
	}

	// Analyse this pulse to see if it changes state to stable or unstable
	switch ( ZChannelData::bZStable )
	{
		case false:
			// check if now stable
			if ( HasZSettled ( ulTimeofLastPulse,  &ZChannelData::ulAvgZInterval ) )
			{
				// calc expected times for (A or B) and Z
				ABChannelData::ulABInterval = ZChannelData::ulAvgZInterval / ABChannelData::uiNPR;
				ABChannelData::ulABMargin 	= ( ABChannelData::ulABInterval * ACCEPTABLE_MARGIN ) / 100L;
				ZChannelData::ulZMargin		= ( ZChannelData::ulAvgZInterval * ACCEPTABLE_MARGIN ) / 100L;
				ZChannelData::bZStable = true;
			}
			break;

		case true:
			// check if still stable
			if ( WithinZMargins ( ulTimeofPulse ) == false )
			{
				ZChannelData::bZStable = false;
				if ( HasZSettled ( ulTimeofLastPulse,  &ZChannelData::ulAvgZInterval ) )
				{
					// calc expected times for (A or B) and Z
					ABChannelData::ulABInterval = ZChannelData::ulAvgZInterval / ABChannelData::uiNPR;
					ABChannelData::ulABMargin = ( ABChannelData::ulABInterval * ACCEPTABLE_MARGIN ) / 100L;
					ZChannelData::bZStable = true;
				}
			}
			break;
	}
	// save this event time
	ulTimeofLastPulse = ulTimeofPulse;
	
}

bool WithinZMargins ( unsigned long ulThisTime )
{
	bool bResult = true;
	if ( ulThisTime > ZChannelData::ulAvgZInterval + ZChannelData::ulZMargin || ulThisTime < ZChannelData::ulAvgZInterval - ZChannelData::ulZMargin )
	{
		bResult = false;
	}
	return bResult;
}
// static void FindStableRev ( void )
// {
// 	if (ZChannelData::bRevRateDetermined == false )
// 	{
// 		ZChannelData::ulLastRevTime = micros();
// 		if ( HasZSettled ( ZChannelData::ulLastRevTime, &ZChannelData::ulAvgZInterval ) == true )
// 		{
// 			// Z has settled down so set the expexted values of each rev and times of A and B channel interrupts
// 			ZChannelData::ulZMargin = ( ZChannelData::ulAvgZInterval * ACCEPTABLE_MARGIN ) / 100L;          // acceptable percent of rev time
// 			ZChannelData::bRevRateDetermined = true;
// 		}
// 	}
// }

// static void MonitorRevStabilty ( void )
// {
// 	// check Z signals stay within margins from expected value
// 	static bool 		 bFirst 		= true;
// 	static unsigned long ulLastTime 	= 0L;
// 	if ( bFirst )
// 	{
// 		// first interrupt so just note the time
// 		ulLastTime = micros();
// 		bFirst = false;
// 	}
// 	else
// 	{
// 		// get interval between this and last interrupt
// 		static unsigned long ulLastAChannelInts = 0UL;
// 		static unsigned long ulLastBChannelInts = 0L;
// 		static unsigned long ulLastRevTime		= 0L;
// 		// keep an eye on rev rate, if it changes the checks will be wrong
// 		unsigned long ulZTime = micros();
// 		if ( ZChannelData::bZStable == true )
// 		{
// 			// see how many A and B channel interrupts occured in this rev
// 			if ( ulLastAChannelInts > 0UL)
// 			{
// 				ABChannelData::ulAChannelInts = ABChannelData::ulAChannelCount - ulLastAChannelInts;
// 			}
// 			ulLastAChannelInts = ABChannelData::ulAChannelCount;
// 			if ( ulLastBChannelInts > 0UL)
// 			{
// 				ABChannelData::ulBChannelInts = ABChannelData::ulBChannelCount - ulLastBChannelInts;
// 			}
// 			ulLastBChannelInts = ABChannelData::ulBChannelCount;       

// 			// check how long this last rev took
// 			ZChannelData::ulZUnstableRevTime =  ulZTime - ulLastRevTime;               				// Time last rev took
// 			long lThisDeviation = ZChannelData::ulZUnstableRevTime - ZChannelData::ulAvgZInterval;      // deviation (in micros) from expected time
// 			if ( abs ( lThisDeviation ) > ZChannelData::ulZMargin )
// 			{
// 				// revs speed has changed by greater than margin
// 				ZChannelData::bZStable = false;
// 			}
// 			else
// 			{
// 				// within acceptable bounds
// 				// if last result has been processed update with this one
// 				if ( ZChannelData::bZResultProcessed == true )
// 				{ 
// 					// calc deviation as +/- percentage from expected average as 100 times too big - reduce on display
// 					ZChannelData::lZRevDeviation = ( lThisDeviation * 10000L ) / ( long ) ZChannelData::ulAvgZInterval;
// 					ZChannelData::bZResultProcessed = false;
// 				}
// 			}    
// 		}
// 		ulLastRevTime = ulZTime;
// 	}
// }

bool HasZSettled ( unsigned long ulThisIntTime, volatile unsigned long * pulAvgRevTime )
{
  static int            iIndexTimeofRevInterrupt  = 0 ;                       	// point to next entry to be used
  static int            iCountInterrupts          = 0;                        	// count of how many entries we have used so far
  static unsigned long  ulTimeofRevInterrupt [ NUM_STABLE_REVS];
  bool                  bSettled                  = false;                    	// true when settled condition met
  // Add latest time to circular array	
  ulTimeofRevInterrupt [ iIndexTimeofRevInterrupt ] = ulThisIntTime;
  iCountInterrupts++;

  // do we have minimum # of interrupt times to start analysis? 
  if ( iCountInterrupts >= NUM_STABLE_REVS )
  {
      // calculate time between oldest and newest interrupt. ( Oldest is next entry in circular array )

      unsigned long ulElapsedTime = ulThisIntTime - ulTimeofRevInterrupt [ ( iIndexTimeofRevInterrupt + 1 ) % NUM_STABLE_REVS ];

      *pulAvgRevTime = ulElapsedTime / ( NUM_STABLE_REVS - 1 );

      // look at prior entries and check that the elapsed times are within (REV_TOLERANCE_FROM_AVG)% of calcuated average
      int iOldest = ( iIndexTimeofRevInterrupt + 1 ) % NUM_STABLE_REVS;
      unsigned long ulMaxDiff = ( *pulAvgRevTime  * REV_TOLERANCE_FROM_AVG ) / 100L;
      bSettled = true;                                  // assume good
      for ( int i = 0 ; i < NUM_STABLE_REVS - 1 ; i++ )
      {
          int iNext = ( iOldest + 1 ) % NUM_STABLE_REVS;
          unsigned long ulInterval = ulTimeofRevInterrupt [ iNext ] - ulTimeofRevInterrupt [ iOldest ];
          long  ulDiff = ulInterval - *pulAvgRevTime;
          if ( abs ( ulDiff ) > ulMaxDiff )
          {
            // not settled
            bSettled = false;
            break;
          }
          iOldest = iNext;
      }
  }
  // move to next entry in circular buffer
  ++iIndexTimeofRevInterrupt %= NUM_STABLE_REVS;
  return bSettled;
}


Encoder::Encoder ( unsigned int uiNPR )
{
	m_uiNPR = uiNPR;				// Number of signals per rev on A or B channel
	ABChannelData::uiNPR;
}

Encoder::~Encoder()
{
	if ( m_pAChannel != nullptr )
	{
		delete m_pAChannel;
	}
	if ( m_pBChannel != nullptr )
	{
		delete m_pBChannel;
	}
	if ( m_pZChannel != nullptr )
	{
		delete m_pZChannel;
	}
}

bool Encoder::AddChannel ( eChannelId channelId, int iPinNum, Channel::ePinMode PinMode, Channel::eSignal Signal )
{
	bool bResult = false;
	// adds a new channel 
	switch ( channelId )
	{
		case ACHANNEL:
			if ( m_pAChannel != nullptr )
			{
				delete m_pAChannel;
			}
			m_pAChannel = new Channel (  iPinNum, PinMode, Signal );
			break;

		case BCHANNEL:
			if ( m_pBChannel != nullptr )
			{
				delete m_pBChannel;
			}
			m_pBChannel = new Channel (  iPinNum, PinMode, Signal );
			break;
		
		case ZCHANNEL:
			if ( m_pZChannel != nullptr )
			{
				delete m_pZChannel;
				ResetZChannelData ();
			}
			m_pZChannel = new Channel (  iPinNum, PinMode, Signal );
			if ( m_pZChannel != nullptr )
			{
				bResult = true;
			}
			//m_pZChannel->StartISR ( ZISR );
			break;
	}
	return bResult;
}



void Encoder::GetState ( String &s )
{
	String sTemp;
	if ( m_pZChannel == nullptr )
	{
		s = "No Z Channel";
	}
	else
	{
		m_pZChannel->Print( sTemp );
		s = sTemp;
	}
}

unsigned int Encoder::GetNPR ()
{
	return m_uiNPR;
} 

// bool CalculateZStableRevRate ()
// {
// 	bool bSuccess = false;
// 	m_pZChannel->StartISR ( FindStableRev );
// 	// wait until stable
// 	do
// 	{
// 		delay ( 1000 );
// 	} while ( ZChannelData::bRevRateDetermined == false );
// 	m_pZChannel->StopISR();
// 	// Stable so now calculate the related A and B timings
// 	ABChannelData::ulABInterval = ZChannelData::ulAvgZInterval / GetNPR();
// 	ABChannelData::ulABMargin = ZChannelData::ulZMargin / GetNPR();
// 	return bSuccess;
// }

// void StartMonitorZStabilty ()
// {
// 	m_pZChannel->StartISR ( MonitorRevStabilty );
// }

unsigned long Encoder::GetChannelZIntFrequencyMicros () { return  m_ulMicrosRevTime; }
unsigned long Encoder::GetChannelAIntFrequencyMicros () { return m_ulMicrosRevTime / GetNPR(); }
unsigned long Encoder::GetChannelBIntFrequencyMicros () { return m_ulMicrosRevTime / GetNPR(); }

void Encoder::ResetZChannelData ()
{
	ZChannelData::ulZMargin				= 0L;	
	ZChannelData::ulAvgZInterval		= 0L;	
	ZChannelData::bRevRateDetermined 	= false;
	ZChannelData::bZStable				= true;
	ZChannelData::ulZUnstableRevTime	= 0UL;	
	ZChannelData::bZResultProcessed		= true;	
	ZChannelData::lZRevDeviation		= 0L;			
}
