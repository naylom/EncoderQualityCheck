/*
   Encoder quality check sketch,  Author Mark Naylor, 2020

   Arduino sketch to help evaluate the quality of signals from a rotary encoder. It is designed to process 3 signals - an A and B incremental signal generated as the encoder turns and an index (Z) signal generated once per revolution

   It uses interrupts to process in the incoming signals and hence requires an Arduino with 3 pins that can generate interrupts. All testing has been done on an Arduino Mega2560.

   Copyright - free to be used at own risk, no warranty given or implied. 

   Processing summary

   Wait until stable rev rate achieved and use this stable rate to check future timings are in keeping with this rate, will halt if rate becomes unstable or event buffer fills.

   Monitors first N revs using Z channel signal to determine rev speed then looks at falling signals on A and B channel to see if the occur unevenly
   i.e. for a 800 p/r encoder would expect to see each A or B channel signal every ( ( 1 / 800 ) / rev per sec ) seconds. 
   If a signal outside this timing occurs it indicates a missed signal or a spurious signal or an encoder that is not made to signal at regular intervals, this creates an event that is logged
   Once each rev, if it is within tolerance the sketch logs an event with actual number of A and B signals seen.

  Ver 1 - Initial release
  Ver 2 - Switched to measuring time with micros from millis
  Ver 3 - at start now wait for INITIAL_REV_WAIT_TIME_SECS seconds before measuring the stability of channels, at end of N seconds use the last LAST_N_REVS times to calculate the avg rev speed
  Ver 4 - Add diagnostics
  Ver 5 - fix bug where time shown when z unstable was not length of unstable rev but the time it occurred
  Ver 6 - Fix bug in calc average time per rev, add code to print Z deviation from expected tim eon each rev
  Ver 7 - further diagnostics
  Ver 8 - only start A & B channels ISR after Z channel is settled down, fix bug where delay in serial output can cause false error in Z report
  ver 9 - fixed bug in A & B Channel ISR - not passing time storage var by ref
  ver 11 - Each rev prints number of channel A & B signals seen during rev. Fixed bug with trying to assign negative value to unsigned var. 
  Ver 12 - changed code to determine if Z has initially settled down by LAST_N_REVS times all being within SETTLE_AVG_MARGIN % of their average rather than waiting arbitrary 10 secs
  Ver 13 - Improved message output on Z pulse to show deviation as correct %'age rather than 100 times too large
  Ver 14 - Create log to hold error events, print errors when log full. This removes any impact of log reporting on event collection.
 */

const static char  sInput[]                             = "Input mode";
const static char  sInputPullup[]                       = "Input with pullup mode ";
const static char  sOutput[]                            = "Ouput Mode";
const static char  sUnknownMode[]                       = "Unknown mode";

// Configuration items
#define COUNT_PER_REV             800UL                         // Number of encoder signals per revolution per A or B channel
#define ACHANNEL_PIN              2
#define ACHANNEL_MODE             INPUT
#define BCHANNEL_PIN              3
#define BCHANNEL_MODE             INPUT
#define ZCHANNEL_PIN              18
#define ZCHANNEL_MODE             INPUT
#define BAUD_RATE                 115200
#define LAST_N_REVS               10                            // How many rev times used at start to get an average
#define ACCEPTABLE_ZMARGIN_PERCENT 5                            // sets the variation allowed in interrupt timings
#define ACCEPTABLE_ABMARGIN_PERCENT 10                          // sets the variation allowed in interrupt timings
#define SETTLE_AVG_MARGIN         5UL                           // percentage of average all settle readings must be within
#define VER                       15

enum     LOG_LEVEL { MINIMAL, MEDIUM, ALL };                    // Valid values for a LOG_LEVEL variable
enum  LOG_LEVEL eLogLevel = MEDIUM;                             // Configured logging Level wanted

volatile bool           bRevRateDetermined            = false;  // true when we have determined the inital average time of a rev
volatile unsigned long  ulAvgZTime                    = 0UL;    // Estimated time for each Z signal based on the average of previous signals that are all within a set % of their mean.
volatile unsigned long  ulZMargin                     = 0UL;    // max micros difference between latest Z signal time and expected average time before Z flagged as unstable
volatile unsigned long  ulZMin                        = 0L;     // min micros accepted for a Z rev
volatile unsigned long  ulZMax                        = 0L;     // max micros accepted for a Z rev
volatile unsigned long  ulABMin                       = 0UL;    // min micros accepted for a A or B pulse
volatile unsigned long  ulABMax                       = 0UL;    // max micros accepted for a A or B pulse
volatile unsigned long  ulAChannelCount               = 0UL;    // Increments each time the A Channel interrupts
volatile unsigned long  ulBChannelCount               = 0UL;    // Increments each time the B Channel interrupts
volatile unsigned long  ulAChannelInts                = 0UL;    // Holds the number of A channel signals seen during last Z channel rev (i.e. between successive Z Channel interrupts)
volatile unsigned long  ulBChannelInts                = 0UL;    // Holds the number of B channel signals seen during last Z channel rev (i.e. between successive Z Channel interrupts)
volatile bool           bZUnstable                    = false;  // true if Z Channel signals at point outside of (expected time +/- error margin)
volatile bool           bLogFull                      = false;  // true if Z Channel signals at point outside of (expected time +/- error margin)
volatile unsigned long  ulZUnstableRevTime            = 0UL;    // Time, in microseconds, between prior Z Channel signal and Z Signal deemed in error
volatile unsigned long  ulTimeofRevInterrupt [ LAST_N_REVS ];   // this used a circular array of times the Z channel interrupted         

         unsigned long  ulABInterval                  = 0UL;    // How often A&B channel should interrupt
         unsigned long  ulABMargin                    = 0UL;    // how much deviation from iABInterval is acceptable

// Create array log for ISR events
typedef struct   
{
  int           iAInterrupts;       // # of A interrupts aince last Z interrupt
  int           iBInterrupts;       // # of B interrupts since last Z interrupt
} ZData;

typedef struct 
{
  bool           bOtherChannelHigh; // true if other channel was HIGH when this event occurred
} ABData;

enum eChannelId { A, B , Z };

struct LOG
{
  unsigned int  uiNextEntry = 0;
  struct LOGENTRY
  {
    unsigned long ulTimeMicros;       // Time event happened
    eChannelId    eChannel;
    unsigned int  ulInterval;        
    union
    {
      ABData  ABEvent;
      ZData   ZEvent;  
    } EventData;
  } LogEntry [ 475 ];
} Logs;
#define NUM_LOGS  ( sizeof ( Logs.LogEntry ) / sizeof  ( Logs.LogEntry [ 0 ] ) )

// Forward declarations
const char* ModetoString (int iMode);
void TerminateProgram (const __FlashStringHelper* pErrMsg);
void ZChannelISR ();
void AChannelISR ();
void BChannelISR ();


bool AddABEvent ( eChannelId eId, unsigned long ulEventTime, unsigned long ulInterval, int iOtherLevel )
{
  bool bResult = true;
  if ( Logs.uiNextEntry < NUM_LOGS )
  {
    Logs.LogEntry [ Logs.uiNextEntry ].ulTimeMicros   = ulEventTime;
    Logs.LogEntry [ Logs.uiNextEntry ].eChannel       = eId;
    Logs.LogEntry [ Logs.uiNextEntry ].ulInterval     = ulInterval;
    Logs.LogEntry [ Logs.uiNextEntry ].EventData.ABEvent.bOtherChannelHigh = iOtherLevel;
    Logs.uiNextEntry++;
  }
  else
  {
    bResult = false;
  }
  
  return bResult;
}

bool AddZEvent  ( unsigned long ulEventTime, unsigned long ulInterval, int iAInts, int iBInts )
{
  bool bResult = true;
  if ( Logs.uiNextEntry < NUM_LOGS )
  {
    Logs.LogEntry [ Logs.uiNextEntry ].ulTimeMicros                   = ulEventTime;
    Logs.LogEntry [ Logs.uiNextEntry ].eChannel                       = Z;
    Logs.LogEntry [ Logs.uiNextEntry ].ulInterval                     = ulInterval;
    Logs.LogEntry [ Logs.uiNextEntry ].EventData.ZEvent.iAInterrupts  = iAInts;
    Logs.LogEntry [ Logs.uiNextEntry ].EventData.ZEvent.iBInterrupts  = iBInts;
    Logs.uiNextEntry++;
  }
  else
  {
    bResult = false;
  }
  return bResult;
}

void PrintEvents ()
{
  unsigned long ulALastEventTime = 0L;
  unsigned long ulBLastEventTime = 0L;
  unsigned long ulZLastEventTime = 0L;
  unsigned int  uiInterval;
  
  for ( int i = 0 ; i < Logs.uiNextEntry ; i++ )
  {
    Serial.print ( GetChannelId ( Logs.LogEntry [ i ].eChannel ) );
    Serial.print ( F ( " event @ " ) );
    Serial.print ( Logs.LogEntry [ i ].ulTimeMicros );
    Serial.print (  F ( ". Interval was " ) );
    Serial.print ( Logs.LogEntry [ i ].ulInterval ) ;
    Serial.print ( F ( ", " ) ) ; 

    switch ( Logs.LogEntry [ i ].eChannel )
    {
      case A:
      case B:
        if ( Logs.LogEntry [ i ].ulInterval < ulABInterval )
        {
            Serial.print ( (long) ( Logs.LogEntry [ i ].ulInterval - ( ulABInterval - ulABMargin ) ) );
        }
        else
        {
          Serial.print ( Logs.LogEntry [ i ].ulInterval - ( ulABInterval + ulABMargin ) );
        }
        Serial.print ( F ( " +/- Margin. Other channel HIGH was " ) ); 
        Serial.println ( Logs.LogEntry [ i ].EventData.ABEvent.bOtherChannelHigh? F ( "true" ) : F ( "false" ) );
        break;

      case Z:
        if ( Logs.LogEntry [ i ].ulInterval < ulAvgZTime )
        {
            Serial.print ( (long) ( Logs.LogEntry [ i ].ulInterval - ( ulAvgZTime - ulZMargin ) ) );
        }
        else
        {
          Serial.print ( Logs.LogEntry [ i ].ulInterval - ( ulAvgZTime + ulZMargin ) );
        }
        
        Serial.print ( F ( " +/- Margin. # A interrupts in interval " ) );
        Serial.print ( Logs.LogEntry [ i ].EventData.ZEvent.iAInterrupts ) ;
        Serial.print ( F ( ", # B interrupts in interval " ) );
        Serial.println ( Logs.LogEntry [ i ].EventData.ZEvent.iBInterrupts ) ;
        break;

      default:
        Serial.println ( F ( "Unknown channel! " ) );
        break;
    }

  }
}

char GetChannelId ( eChannelId channel )
{
  char cResult;
  switch ( channel )
  {
    case A:
      cResult = 'A';
      break;

    case B:
      cResult = 'B';
      break;

    case Z:
      cResult = 'Z';
      break;

    default:
      cResult = '?';
      break;
  }
  return cResult;
}

void setup ()
{
  Serial.begin (BAUD_RATE);
  // Print config state for this run
  Serial.print  ( F ("Encoder Quality Check Ver. " ) );
  Serial.println ( VER );
  Serial.print ( F ( "Serial output running at " ) );
  Serial.println ( BAUD_RATE );
  Serial.print ( F ( "Channel A expected on Digital Pin " ) );
  Serial.print ( ACHANNEL_PIN );
  Serial.print ( F (", connected to interrupt " ) );
  Serial.print ( digitalPinToInterrupt ( ACHANNEL_PIN ) );
  Serial.print ( F ( ", mode is " ) );
  Serial.println ( ModetoString ( ACHANNEL_MODE ) );
  Serial.print ( F ( "Channel B expected on Digital Pin " ) );
  Serial.print ( BCHANNEL_PIN);
  Serial.print ( F ( ", connected to interrupt "));
  Serial.print ( digitalPinToInterrupt ( BCHANNEL_PIN ) );
  Serial.print ( F ( ", mode is " ) );
  Serial.println ( ModetoString ( BCHANNEL_MODE ) );
  Serial.print ( F ( "Channel Z expected on Digital Pin " ) );
  Serial.print ( ZCHANNEL_PIN );
  Serial.print ( F ( ", connected to interrupt " ) );
  Serial.print ( digitalPinToInterrupt ( ZCHANNEL_PIN ) );
  Serial.print ( F ( ", mode is " ) );
  Serial.println ( ModetoString ( ZCHANNEL_MODE ) );
  Serial.println ();

  // Sanity check config before implementing it
  if (ACHANNEL_PIN == BCHANNEL_PIN || ACHANNEL_PIN == ZCHANNEL_PIN || BCHANNEL_PIN == ZCHANNEL_PIN)
  { 
    TerminateProgram ( F ( "Invalid Config - expected three input different pins for the A,B and Z channels" ) );
  }
  else
  {
    if (digitalPinToInterrupt ( ACHANNEL_PIN ) == -1 || digitalPinToInterrupt ( BCHANNEL_PIN ) == -1 || digitalPinToInterrupt ( ZCHANNEL_PIN ) == -1)
    {
      TerminateProgram ( F ( "Invalid Config - not all pins selected for channels support interrupts" ) );
    }
    else
    {
      if ( strcmp ( ModetoString ( ACHANNEL_MODE ), sUnknownMode) == 0 || strcmp ( ModetoString (BCHANNEL_MODE), sUnknownMode) == 0 || strcmp ( ModetoString (ZCHANNEL_MODE), sUnknownMode) == 0)
      {
        TerminateProgram ( F ( "Invalid Config - channel pins should be set to one of: INPUT / INPUT_PULLUP / OUTPUT" ) );
      }
    }
  }

  // Apply config
  pinMode (ACHANNEL_PIN, ACHANNEL_MODE);        // internal input pin 

  pinMode (BCHANNEL_PIN, BCHANNEL_MODE);        // internal input pin

  pinMode (ZCHANNEL_PIN, ZCHANNEL_MODE);        // internal pullup input pin

  // Setting up interrupts


  // Z falling signal from encoder will invoke ZChannelISR(). 
  ulTimeofRevInterrupt [ 0 ] = 0UL;
  attachInterrupt (digitalPinToInterrupt (ZCHANNEL_PIN), ZChannelISR, FALLING);

  Serial.println ( F ( "Waiting for stable rev rate" )  );

  // Wait for stable revs to occur
  bool bDoOnce = false;
  do
  {
    delay ( 100 );
    if ( bDoOnce == false && ulTimeofRevInterrupt [ 0 ] != 0UL )
    {
      bDoOnce = true;
      Serial.println (  F ( "Started to receive Z signals" ) );
    }   
  }
  while ( bRevRateDetermined == false );
  // Z has settled so now start monitoring A and B channels
  Serial.println ( F ( "Started monitoring for events" ) );

  // A falling signal from encoder will invoke AChannelISR(). 
  attachInterrupt (digitalPinToInterrupt (ACHANNEL_PIN), AChannelISR, FALLING);

  // B falling signal from encoder will invoke BChannelISR().
  attachInterrupt (digitalPinToInterrupt (BCHANNEL_PIN), BChannelISR, FALLING);
  
  // Now show calculated rev speed
  Serial.print ( F ( "Recent revs are taking an average of ") );
  Serial.print ( ulAvgZTime );
  Serial.print ( F ( " microseconds. Will accept between " ) );
  Serial.print ( ulZMin );
  Serial.print ( F ( " and " ) );
  Serial.print ( ulZMax );
  Serial.print ( F ( " microseconds.\nEach A or B channel signal should occur every " ) );
  Serial.print ( ulABInterval );
  Serial.print ( F ( " microseconds. Will accept between " ) );
  Serial.print ( ulABMin );
  Serial.print ( F ( " and " ) );
  Serial.print ( ulABMax );
  Serial.println ( F ( " microseconds." ) );
  Serial.print ( F ( "This implies a rpm of " ) );
  Serial.println ( ( 1000000L * 60L ) / ulAvgZTime );
  
  Serial.println();
  Serial.print ( F ( "Rev times used for avg : " ) );
  for ( int i = 0 ; i < LAST_N_REVS ; i++ )
  {       
    if ( i > 0 )
    {
      Serial.print ( F ( ", ") );
    }
    Serial.print ( ulTimeofRevInterrupt [ i ] );
  }
  Serial.println();
}

void loop ()
{
  // check for a rev outside of expected tolerance; if found tell user and quit
  if ( bZUnstable == true || bLogFull == true )
  {
    detachInterrupt ( digitalPinToInterrupt (ACHANNEL_PIN ) );
    detachInterrupt ( digitalPinToInterrupt (BCHANNEL_PIN ) );
    detachInterrupt ( digitalPinToInterrupt (ZCHANNEL_PIN ) );
    if ( bLogFull )
    {
      Serial.println ( F ( "Log full" ) );
    }
    else
    {
      Serial.println ( F ( "Revs now unstable" ) );
    }
    PrintEvents();

    TerminateProgram ( F ( "Program ending" ) );  
  }
}

const char* ModetoString (int iMode)
{
  const static char* pResult;
  switch (iMode)
  {
  case INPUT:
    pResult = sInput;
    break;

  case INPUT_PULLUP:
    pResult = sInputPullup;
    break;

  case OUTPUT:
    pResult = sOutput;
    break;

  default:
    pResult = sUnknownMode;
    break;
  }
  return pResult;
}

void TerminateProgram (const __FlashStringHelper* pErrMsg)
{
  Serial.println (pErrMsg);
  Serial.flush ();
  exit (0);
}

void AChannelISR ()
{
  // check how long since last interrupt
  static unsigned long ulALastTime = 0UL;
  static unsigned long ulInterval;

  ulAChannelCount++;

  if ( CheckElapsedTime ( &ulALastTime, &ulInterval) == false )
  {
    if ( AddABEvent ( A, ulALastTime, ulInterval, digitalRead ( BCHANNEL_PIN ) )  == false )
    {
      // full so stop program
      bLogFull = true;
    }
  }
}

void BChannelISR ()
{
  // check how long since last interrupt
  static unsigned long ulBLastTime = 0UL;
  static unsigned long ulInterval;
  ulBChannelCount++;

  if ( CheckElapsedTime ( &ulBLastTime, &ulInterval) == false )
  {
    if ( AddABEvent ( B, ulBLastTime, ulInterval, digitalRead ( ACHANNEL_PIN ) ) == false )
    {
      // full so stop program
      bLogFull = true;
    }
  }

}

/*
* This function looks at the time between signals on the A or B Channel and returns false if it is outside the accepted margin

Parameters
pulLastTime - pointer to the time of the last signal on this channel in microseconds
plOverUnder - pointer to storeage to hold the number of microseconds that at above/below the expected interval
*/
bool CheckElapsedTime ( unsigned long *pulLastTime, unsigned long * pulOverUnder )
{
  bool bResult        = true;              // All OK
  unsigned long ulNow = micros();

  // see if not first time this has run
  if ( *pulLastTime > 0UL )
  {
    // ignore this time if micros() has reset
    if ( ulNow > *pulLastTime )
    {
      // if we have a target then compare it
      if ( ulABMargin > 0UL )
      {
        // see how much time has elapsed since last interrupt
        *pulOverUnder = ulNow - *pulLastTime;
        if ( *pulOverUnder > ( ulABMax ) || *pulOverUnder < ( ulABMin ) )
        {
            // error, took too long or too soon
            bResult = false;
        }
      }
    }
  }
  *pulLastTime = ulNow;
  return bResult;
}

// ZChannelISR is activated if DigitalPin ZCHANNEL_PIN is going from HIGH to LOW
// Initially keep testing time of latest signal to see if the rate has settled down then check it stays at that rate
// Also keep track of the number of pulses recorded on A and B channels that occur between each Z channel signal
void ZChannelISR ()
{
  enum    bZState { BEGIN_CALC_REV_RATE, CALC_REV_RATE, MONITOR_REV_RATE };         // valid states for Z Channel logic
  static  bZState           ZRevState           = BEGIN_CALC_REV_RATE;              // current state of Z channel logic
  static  int               iRevCount           = 1;                                // Index of circular buffer to hold time of next interrupt
  static  unsigned long     ulLastRevTime       = 0UL;                              // Time, in microseconds, that last Z channel interrupted
  static  unsigned long     ulLastAChannelInts  = 0UL;
  static  unsigned long     ulLastBChannelInts  = 0UL;
  
  switch ( ZRevState )
  {
    case BEGIN_CALC_REV_RATE:
      ulLastRevTime = micros();
      if ( ZSettled ( ulLastRevTime, &ulAvgZTime ) == true )
      {
        // Z has settled down so set the expexted values of each rev and times of A and B channel interrupts
        ulZMargin = ( ulAvgZTime * ACCEPTABLE_ZMARGIN_PERCENT ) / 100UL;           // acceptable percent of rev time
        ulZMax = ulAvgZTime + ulZMargin;
        ulZMin = ulAvgZTime - ulZMargin;
        ulABInterval = ulAvgZTime / COUNT_PER_REV;
        ulABMargin = ( ulAvgZTime * ACCEPTABLE_ABMARGIN_PERCENT ) / 100UL / COUNT_PER_REV ;
        ulABMin = ulABInterval - ulABMargin;
        ulABMax = ulABInterval + ulABMargin;
        bRevRateDetermined = true;
        ZRevState = MONITOR_REV_RATE;
      }
      break;

    case MONITOR_REV_RATE:
      // // keep an eye on rev rate, if it changes the checks will be wrong

      unsigned long ulNow = micros();
      ulAChannelInts = ulAChannelCount - ulLastAChannelInts;
      ulLastAChannelInts = ulAChannelCount;
      ulBChannelInts = ulBChannelCount - ulLastBChannelInts;
      ulLastBChannelInts = ulBChannelCount; 
      // log if outside of accepted margins
      if ( ( ulNow - ulLastRevTime ) > ulZMax || ( ulNow - ulLastRevTime ) < ulZMin )
      {
        if ( AddZEvent ( ulNow, ulNow - ulLastRevTime, ulAChannelInts, ulBChannelInts ) == false )
        {
            // full, so stop
            bLogFull = true;
        }
      }
      // check if unstable
      ulZUnstableRevTime =  ulNow - ulLastRevTime;                // Time last rev took
      long lThisDeviation = ulZUnstableRevTime - ulAvgZTime;       // deviation (in micros) from expected time
      if ( abs ( lThisDeviation ) > ulZMargin )
      {
          // revs speed has changed by greater than margin
        bZUnstable = true;
      }
      ulLastRevTime = ulNow;
      break;
  }
}

/*
 * function to determine if Z has initially stabilised and we can set the expected rev times and by implication the interval between interrupts on the A and B channels
 * this is called for each Z interrupt at the start of the program
 * 
 * This fills a circular buffer with the time of the last LAST_N_REVS interrupts and once all entries have a value it calculates the time between the oldest and newest. From this 
 * the average interval time is calculated. This is deemed settled if all entries only deviate by +/- SETTLE_AVG_MARGIN % from the calculated average.
 * 
 * Parameters
 * ulThisIntTime - time this interrupt occured in microseconds
 * pulAvgRevTime - pointer to var to hold the average time between interrupts once they have settled down
 * 
 * Returns true if settled state is reached, pulAvgRevTime will be populated otherwise its value is undefined.
*/
bool ZSettled ( unsigned long ulThisIntTime, volatile unsigned long * pulAvgRevTime )
{
  static int            iIndexTimeofRevInterrupt  = 0 ;                       // point to next entry to be used
  static int            iCountInterrupts          = 0;                        // count of how many entries we have used so far
  bool                  bSettled                  = false;                    // true when settled condition met
  // Add latest time to circular array
  ulTimeofRevInterrupt [ iIndexTimeofRevInterrupt ] = ulThisIntTime;
  iCountInterrupts++;

  // do we have minimum # of interrupt times to start analysis? 
  if ( iCountInterrupts >= LAST_N_REVS )
  {
      // calculate time between oldest and newest interrupt. ( Oldest is next entry in circular array )

      unsigned long ulElapsedTime = ulThisIntTime - ulTimeofRevInterrupt [ ( iIndexTimeofRevInterrupt + 1 ) % LAST_N_REVS ];

      *pulAvgRevTime = ulElapsedTime / ( LAST_N_REVS - 1 );

      // look at prior entries and check that the elapsed times are within 5% of calcuated average
      int iOldest = ( iIndexTimeofRevInterrupt + 1 ) % LAST_N_REVS;
      unsigned long ulMaxDiff = ( *pulAvgRevTime  * SETTLE_AVG_MARGIN ) / 100UL;
      bSettled = true;                                  // assume good
      for ( int i = 0 ; i < LAST_N_REVS - 1 ; i++ )
      {
          int iNext = ( iOldest + 1 ) % LAST_N_REVS;
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
  ++iIndexTimeofRevInterrupt %= LAST_N_REVS;
  return bSettled;
}
