/*
   Encoder setup check sketch,  Author Mark Naylor, 2020

   Arduino sketch to help check the inputs are being seen correctly before using the encoder quality check sketch

   First identifies which of first 21 digital pins will support interrupts then looks (for 15 seconds) for signals with pins in INPUT_PULLUP mode 
   
   and then checks any remaining pins (for another 15 seconds) using INPUT if any signals subsequently seen. Stops after 3 or more pins seen genertaing interrupts

   It uses interrupts to process in the incoming signals and hence requires an Arduino with 3 pins that can generate interrupts. All testing has been done on an Arduino Mega2560.

   Copyright - free to be used at own risk, no warranty given or implied. 

  Ver 1 - Initial release

 */

const static char  sInput[]                             = "Input mode";
const static char  sInputPullup[]                       = "Input with pullup mode";
const static char  sUnknown[]                           = "Unexpected mode";

// forward declarations
void ISR0 ();
void ISR1 ();
void ISR2 ();
void ISR3 ();
void ISR4 ();
void ISR5 ();
void ISR6 ();
void ISR7 ();
void ISR8 ();
void ISR9 ();
void ISR10 ();
void ISR11 ();
void ISR12 ();
void ISR13 ();
void ISR14 ();
void ISR15 ();
void ISR16 ();
void ISR17 ();
void ISR18 ();
void ISR19 ();
void ISR20 ();
void ISR21 ();
 
void (*ListISR [] )(void)  = 
{
  ISR0,
  ISR1,
  ISR2,
  ISR3,
  ISR4,
  ISR5,
  ISR6,
  ISR7,
  ISR8,
  ISR9,
  ISR10,
  ISR11,
  ISR12,
  ISR13,
  ISR14,
  ISR15,
  ISR16,
  ISR17,
  ISR18,
  ISR19,
  ISR20,
  ISR21
};
#define MAX_PINS_TO_CHECK         min ( NUM_DIGITAL_PINS, sizeof ( ListISR ) / sizeof ( ListISR[0] ) )

// Configuration items
#define BAUD_RATE                 115200
#define VER                       1


// Create struct to monitor pin results

typedef struct 
{
  int           iPinNum;
  bool          bHasBeenSignalled;                             // true if signals seen on this pin
  int           iModeWhenSignalled;   
  void         (*pISR)();
} PINDATA;

volatile typedef struct 
{
  int           iNumPins;                                     // num of entries in sPinList that are valid
  PINDATA       sPinList [ MAX_PINS_TO_CHECK ];
} PININFO;

PININFO sPinInfo;

volatile int iPinMode;                                               // current mode of pins being tested



void setup ()
{
  Serial.begin (BAUD_RATE);
  // Print config state for this run
  Serial.print  ( F ("Encoder Setup Check Ver. " ) );
  Serial.println ( VER );
  Serial.print ( F ( "Serial output running at " ) );
  Serial.println ( BAUD_RATE );

  if ( NUM_DIGITAL_PINS > MAX_PINS_TO_CHECK  )
  {
    Serial.print ( F ( "Compatibility warning: This program will only check the first " ) );
    Serial.print ( MAX_PINS_TO_CHECK );
    Serial.print ( F ( " digital pins, your machine has " ) );
    Serial.println ( NUM_DIGITAL_PINS );
  }
  Serial.println();
  // Initialise pin data structure
  InitPinInfo ( &sPinInfo );
  
  Serial.print ( sPinInfo.iNumPins );
  Serial.println ( F ( " pin(s) can be used for interrupts as follows:\nPin #\tInt Vector" ) );
  for ( int i = 0 ; i < sPinInfo.iNumPins ; i++ )
  {
    Serial.print ( sPinInfo.sPinList [ i ].iPinNum );
    Serial.print ( F ( "\t" ) );
    Serial.println ( digitalPinToInterrupt ( sPinInfo.sPinList [ i ].iPinNum ) );
  }

  Serial.println ( F ( "\nStarting to look for signals on pins" ) );
  
  Serial.println();
}

void loop ()
{
  static int iCountSec = 0;
  switch  ( iCountSec )
  {
     case 0:
      // put in PULLUP mode
      Serial.println ();
      Serial.println ( F ( "Checking INPUT_PULLUP mode" ) ) ;
      
      StopISRs( &sPinInfo );
      iPinMode = INPUT_PULLUP;
      SetPinsMode ( &sPinInfo, INPUT_PULLUP );
      StartISRs ( &sPinInfo );
      
      Serial.println ( F ( "Please turn encoder" ) );
      delay ( 1000 );
      break;
      
    case 15:
      // put in input mode
      Serial.println ( F ( "Checking INPUT mode" ) ) ;
      
      StopISRs( &sPinInfo );
      iPinMode = INPUT;
      SetPinsMode ( &sPinInfo, INPUT );
      StartISRs ( &sPinInfo );

      Serial.println ( F ( "Please continmue to turn encoder" ) );
      delay ( 1000 );
      break;
      
    case 30:
      // end of program
      Serial.println ();
      Serial.println ( F ( "Finished testing" ) );
      DisplayResults ( &sPinInfo );
      TerminateProgram ( F ( "Program ending" ) );
      break;

    default:
      // check if 3 pins have signalled
      Serial.print ( F ( "." ) );
      if ( NumPinsSignalled ( &sPinInfo ) >= 3 )
      {
        DisplayResults ( &sPinInfo );
        TerminateProgram ( F ( "3 pins found, terminating program" ) );
      }
      delay ( 1000 );
      break;
  }
  iCountSec++;
        
}

void InitPinInfo ( PININFO * psPinInfo )
{
  for ( int i = 0 ; i < MAX_PINS_TO_CHECK ; i++ )
  {
    psPinInfo->sPinList [ i ].iPinNum = -1;
    psPinInfo->sPinList [ i ].bHasBeenSignalled = false;
    psPinInfo->sPinList [ i ].iModeWhenSignalled = -1;
    psPinInfo->sPinList [ i ].pISR = 0;
  }
  psPinInfo->iNumPins = 0;

  // determine which pins can support interrupts   
  for ( int i = 0 ; i < MAX_PINS_TO_CHECK ; i++ )
  {
    if ( digitalPinToInterrupt ( i ) != -1 )
    { 
      psPinInfo->sPinList [ sPinInfo.iNumPins ].iPinNum = i;
      psPinInfo->sPinList [ sPinInfo.iNumPins++ ].pISR = ListISR [ i ] ;
    }
  }
}

int NumPinsSignalled ( PININFO * psPinInfo  )
{
  int iResult = 0;
  for ( int i = 0 ; i < psPinInfo->iNumPins ; i++ )
  {
    if ( psPinInfo->sPinList [ i ].bHasBeenSignalled == true )
    {
      iResult++;
    }
  }
  return iResult;
}

void DisplayResults ( PININFO * psPinInfo )
{
  for ( int i = 0 ; i < psPinInfo->iNumPins ; i++ )
  {
    Serial.print ( F ( "\n Pin " ) );
    Serial.print ( psPinInfo->sPinList [ i ].iPinNum );
    Serial.print ( F ( " attached to interrupt " ) );
    Serial.print ( digitalPinToInterrupt ( psPinInfo->sPinList [ i ].iPinNum ) );
    if ( psPinInfo->sPinList [ i ].bHasBeenSignalled == true )
    {
      Serial.print ( F ( " signalled when pin was set to mode : " ) );
      Serial.println ( ModetoString ( psPinInfo->sPinList [ i ].iModeWhenSignalled ) );
    }
    else
    {
      Serial.print ( F ( " did not signal" ) );
    }
    Serial.println();
  }
}
 
void StartISRs ( PININFO * psPinInfo )
{
  for ( int i = 0 ; i < psPinInfo->iNumPins ; i++ )
  {
    attachInterrupt ( digitalPinToInterrupt ( psPinInfo->sPinList [ i ].iPinNum ), psPinInfo->sPinList [ i ].pISR, FALLING);
  }
}

void StopISRs ( PININFO * psPinInfo )
{
  for ( int i = 0 ; i < psPinInfo->iNumPins ; i++ )
  {
    detachInterrupt ( digitalPinToInterrupt ( psPinInfo->sPinList [ i ].iPinNum ) );
  }
}


void SetPinsMode ( PININFO * psPinInfo, int iMode )
{
  for ( int i = 0 ; i < psPinInfo->iNumPins ; i++ )
  {
    if ( psPinInfo->sPinList [ i ].bHasBeenSignalled == false )
    {
      psPinInfo->sPinList [ i ].iModeWhenSignalled = iMode;
      pinMode (  psPinInfo->sPinList [ i ].iPinNum , iMode);        
    }
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

  default:
    pResult = sUnknown;
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

void BaseISRCode ( int iIndex )
{
  for ( int i = 0 ; i < sPinInfo.iNumPins ; i++ )
  {
    if ( sPinInfo.sPinList [ i ].iPinNum == iIndex )
    {
      // Set has signalled to true
      sPinInfo.sPinList [ i ].bHasBeenSignalled = true;
      sPinInfo.sPinList [ i ].iModeWhenSignalled = iPinMode;
    }
  }
}

void ISR0 () {  BaseISRCode ( 0 ); }
void ISR1 () {  BaseISRCode ( 1 ); }
void ISR2 () {  BaseISRCode ( 2 ); }
void ISR3 () {  BaseISRCode ( 3 ); }
void ISR4 () {  BaseISRCode ( 4 ); }
void ISR5 () {  BaseISRCode ( 5 ); }
void ISR6 () {  BaseISRCode ( 6 ); }
void ISR7 () {  BaseISRCode ( 7 ); }
void ISR8 () {  BaseISRCode ( 8 ); }
void ISR9 () {  BaseISRCode ( 9 ); }
void ISR10 () {  BaseISRCode ( 10 ); }
void ISR11 () {  BaseISRCode ( 11 ); }
void ISR12 () {  BaseISRCode ( 12 ); }
void ISR13 () {  BaseISRCode ( 13 ); }
void ISR14 () {  BaseISRCode ( 14 ); }
void ISR15 () {  BaseISRCode ( 15 ); }
void ISR16 () {  BaseISRCode ( 16 ); }
void ISR17 () {  BaseISRCode ( 17 ); }
void ISR18 () {  BaseISRCode ( 18 ); }
void ISR19 () {  BaseISRCode ( 19 ); }
void ISR20 () {  BaseISRCode ( 20 ); }
void ISR21 () {  BaseISRCode ( 21 ); }


