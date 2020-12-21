Channel::Channel ( int iPin, ePinMode PinMode, eSignal Signal )
{
  m_iPinMode = (int) PinMode;
  m_iPin = iPin;
  m_iSignalMode = (int) Signal;
  m_eChannelStatus = INACTIVE;
  if ( digitalPinToInterrupt ( m_iPin ) == NOT_AN_INTERRUPT )
  {
    m_eChannelStatus = BAD_CONFIG;
  }
  if ( m_iPinMode != INPUT && m_iPinMode != INPUT_PULLUP && m_iPinMode != OUTPUT )
  {
    m_eChannelStatus = BAD_CONFIG;
  }
}

Channel::~Channel ()
{
    if ( m_eChannelStatus == ACTIVE )
    {
      // stop ISR 
      detachInterrupt ( digitalPinToInterrupt ( m_iPin) );
    }
}

Channel::ChannelStatus Channel::Status ()
{
  return m_eChannelStatus;
}

Channel::ePinMode Channel::GetPinMode()
{
  return (ePinMode) m_iPinMode;
}

Channel::eSignal Channel::GetSignal()
{
  return (eSignal) m_iSignalMode;
}

void Channel::Print (String &sResult )
{
  //String sResult;
  sResult = "Using Pin " + m_iPin;
  // sResult += " in mode " + m_iPinMode;
  // sResult += ". This will generate interrupts on vector " + digitalPinToInterrupt ( m_iPin );
  // sResult += ", ISR is ";
  // sResult += m_bISRActive == false ? "inactive" : "active";
  // if ( m_bISRActive == true )
  // {
  //   sResult += " and triggers on " + m_iSignalMode;
  // }
  //return sResult;
}

bool Channel::StartISR ( void pISR(void) )
{
  bool bResult = false;
  // validate signal mode
  if ( m_eChannelStatus != BAD_CONFIG )
  {
    bResult = true;
    // see if ISR already set and remove if present
    StopISR();
    
    pinMode ( m_iPin, m_iPinMode );
    m_bISRActive = true;
    attachInterrupt ( digitalPinToInterrupt ( m_iPin ), pISR, m_iSignalMode );
  }
  return bResult;
}

bool Channel::StopISR ()
{
  bool bResult = false;
  if ( m_bISRActive )
  {
    detachInterrupt ( digitalPinToInterrupt ( m_iPin) );
    m_bISRActive = false;
    bResult = true;
  }
  return bResult;
}
