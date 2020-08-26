/*
  twi.c - TWI/I2C library for Wiring & Arduino
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 2012 by Todd Krein (todd@krein.org) to implement repeated starts
  Modified 2020 by Greyson Christoforo (grey@christoforo.net) to implement timeouts
*/

#include <math.h>
#include <stdlib.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
//#include <compat/mytwi.h>
#include <compat/twi.h> // good one ;p
#include "Arduino.h" // for digitalWrite and micros

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif

#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif

#include "pins_arduino.h"
#include "mytwi.h"

static volatile uint8_t mytwi_state;
static volatile uint8_t mytwi_slarw;
static volatile uint8_t mytwi_sendStop;			// should the transaction end with a stop
static volatile uint8_t mytwi_inRepStart;			// in the middle of a repeated start

// twi_timeout_us > 0 prevents the code from getting stuck in various while loops here
// if twi_timeout_us == 0 then timeout checking is disabled (the previous Wire lib behavior)
// at some point in the future, the default twi_timeout_us value could become 25000
// and twi_do_reset_on_timeout could become true
// to conform to the SMBus standard
// http://smbus.org/specs/SMBus_3_1_20180319.pdf
static volatile uint32_t mytwi_timeout_us = 0ul;
static volatile bool mytwi_timed_out_flag = false;  // a timeout has been seen
static volatile bool mytwi_do_reset_on_timeout = false;  // reset the TWI registers on timeout

static void (*mytwi_onSlaveTransmit)(void);
static void (*mytwi_onSlaveReceive)(uint8_t*, int);

static uint8_t mytwi_masterBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t mytwi_masterBufferIndex;
static volatile uint8_t mytwi_masterBufferLength;

static uint8_t mytwi_txBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t mytwi_txBufferIndex;
static volatile uint8_t mytwi_txBufferLength;

static uint8_t mytwi_rxBuffer[TWI_BUFFER_LENGTH];
static volatile uint8_t mytwi_rxBufferIndex;

static volatile uint8_t mytwi_error;

/*
 * Function twi_init
 * Desc     readys twi pins and sets twi bitrate
 * Input    none
 * Output   none
 */
void mytwi_init(void)
{
  // initialize state
  mytwi_state = TWI_READY;
  mytwi_sendStop = true;		// default value
  mytwi_inRepStart = false;

  // activate internal pullups for twi.
  digitalWrite(SDA, 1);
  digitalWrite(SCL, 1);

  // initialize twi prescaler and bit rate
  cbi(TWSR, TWPS0);
  cbi(TWSR, TWPS1);
  TWBR = ((F_CPU / TWI_FREQ) - 16) / 2;

  /* twi bit rate formula from atmega128 manual pg 204
  SCL Frequency = CPU Clock Frequency / (16 + (2 * TWBR))
  note: TWBR should be 10 or higher for master mode
  It is 72 for a 16mhz Wiring board with 100kHz TWI */

  // enable twi module, acks, and twi interrupt
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA);
}

/*
 * Function twi_disable
 * Desc     disables twi pins
 * Input    none
 * Output   none
 */
void mytwi_disable(void)
{
  // disable twi module, acks, and twi interrupt
  TWCR &= ~(_BV(TWEN) | _BV(TWIE) | _BV(TWEA));

  // deactivate internal pullups for twi.
  digitalWrite(SDA, 0);
  digitalWrite(SCL, 0);
}

/*
 * Function twi_slaveInit
 * Desc     sets slave address and enables interrupt
 * Input    none
 * Output   none
 */
void mytwi_setAddress(uint8_t address)
{
  // set twi slave address (skip over TWGCE bit)
  TWAR = address << 1;
}

/*
 * Function twi_setClock
 * Desc     sets twi bit rate
 * Input    Clock Frequency
 * Output   none
 */
void mytwi_setFrequency(uint32_t frequency)
{
  TWBR = ((F_CPU / frequency) - 16) / 2;

  /* twi bit rate formula from atmega128 manual pg 204
  SCL Frequency = CPU Clock Frequency / (16 + (2 * TWBR))
  note: TWBR should be 10 or higher for master mode
  It is 72 for a 16mhz Wiring board with 100kHz TWI */
}

/*
 * Function twi_readFrom
 * Desc     attempts to become twi bus master and read a
 *          series of bytes from a device on the bus
 * Input    address: 7bit i2c device address
 *          data: pointer to byte array
 *          length: number of bytes to read into array
 *          sendStop: Boolean indicating whether to send a stop at the end
 * Output   number of bytes read
 */
uint8_t mytwi_readFrom(uint8_t address, uint8_t* data, uint8_t length, uint8_t sendStop)
{
  uint8_t i;

  // ensure data will fit into buffer
  if(TWI_BUFFER_LENGTH < length){
    return 0;
  }

  // wait until twi is ready, become master receiver
  uint32_t startMicros = micros();
  while(TWI_READY != mytwi_state){
    if((mytwi_timeout_us > 0ul) && ((micros() - startMicros) > mytwi_timeout_us)) {
      mytwi_handleTimeout(mytwi_do_reset_on_timeout);
      return 0;
    }
  }
  mytwi_state = TWI_MRX;
  mytwi_sendStop = sendStop;
  // reset error state (0xFF.. no error occured)
  mytwi_error = 0xFF;

  // initialize buffer iteration vars
  mytwi_masterBufferIndex = 0;
  mytwi_masterBufferLength = length-1;  // This is not intuitive, read on...
  // On receive, the previously configured ACK/NACK setting is transmitted in
  // response to the received byte before the interrupt is signalled.
  // Therefor we must actually set NACK when the _next_ to last byte is
  // received, causing that NACK to be sent in response to receiving the last
  // expected byte of data.

  // build sla+w, slave device address + w bit
  mytwi_slarw = TW_READ;
  mytwi_slarw |= address << 1;

  if (true == mytwi_inRepStart) {
    // if we're in the repeated start state, then we've already sent the start,
    // (@@@ we hope), and the TWI statemachine is just waiting for the address byte.
    // We need to remove ourselves from the repeated start state before we enable interrupts,
    // since the ISR is ASYNC, and we could get confused if we hit the ISR before cleaning
    // up. Also, don't enable the START interrupt. There may be one pending from the
    // repeated start that we sent ourselves, and that would really confuse things.
    mytwi_inRepStart = false;			// remember, we're dealing with an ASYNC ISR
    startMicros = micros();
    do {
      TWDR = mytwi_slarw;
      if((mytwi_timeout_us > 0ul) && ((micros() - startMicros) > mytwi_timeout_us)) {
        mytwi_handleTimeout(mytwi_do_reset_on_timeout);
        return 0;
      }
    } while(TWCR & _BV(TWWC));
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	// enable INTs, but not START
  } else {
    // send start condition
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTA);
  }

  // wait for read operation to complete
  startMicros = micros();
  while(TWI_MRX == mytwi_state){
    if((mytwi_timeout_us > 0ul) && ((micros() - startMicros) > mytwi_timeout_us)) {
      mytwi_handleTimeout(mytwi_do_reset_on_timeout);
      return 0;
    }
  }

  if (mytwi_masterBufferIndex < length) {
    length = mytwi_masterBufferIndex;
  }

  // copy twi buffer to data
  for(i = 0; i < length; ++i){
    data[i] = mytwi_masterBuffer[i];
  }

  return length;
}

/*
 * Function twi_writeTo
 * Desc     attempts to become twi bus master and write a
 *          series of bytes to a device on the bus
 * Input    address: 7bit i2c device address
 *          data: pointer to byte array
 *          length: number of bytes in array
 *          wait: boolean indicating to wait for write or not
 *          sendStop: boolean indicating whether or not to send a stop at the end
 * Output   0 .. success
 *          1 .. length to long for buffer
 *          2 .. address send, NACK received
 *          3 .. data send, NACK received
 *          4 .. other twi error (lost bus arbitration, bus error, ..)
 *          5 .. timeout
 */
uint8_t mytwi_writeTo(uint8_t address, uint8_t* data, uint8_t length, uint8_t wait, uint8_t sendStop)
{
  uint8_t i;

  // ensure data will fit into buffer
  if(TWI_BUFFER_LENGTH < length){
    return 1;
  }

  // wait until twi is ready, become master transmitter
  uint32_t startMicros = micros();
  while(TWI_READY != mytwi_state){
    if((mytwi_timeout_us > 0ul) && ((micros() - startMicros) > mytwi_timeout_us)) {
      mytwi_handleTimeout(mytwi_do_reset_on_timeout);
      return (5);
    }
  }
  mytwi_state = TWI_MTX;
  mytwi_sendStop = sendStop;
  // reset error state (0xFF.. no error occured)
  mytwi_error = 0xFF;

  // initialize buffer iteration vars
  mytwi_masterBufferIndex = 0;
  mytwi_masterBufferLength = length;

  // copy data to twi buffer
  for(i = 0; i < length; ++i){
    mytwi_masterBuffer[i] = data[i];
  }

  // build sla+w, slave device address + w bit
  mytwi_slarw = TW_WRITE;
  mytwi_slarw |= address << 1;

  // if we're in a repeated start, then we've already sent the START
  // in the ISR. Don't do it again.
  //
  if (true == mytwi_inRepStart) {
    // if we're in the repeated start state, then we've already sent the start,
    // (@@@ we hope), and the TWI statemachine is just waiting for the address byte.
    // We need to remove ourselves from the repeated start state before we enable interrupts,
    // since the ISR is ASYNC, and we could get confused if we hit the ISR before cleaning
    // up. Also, don't enable the START interrupt. There may be one pending from the
    // repeated start that we sent outselves, and that would really confuse things.
    mytwi_inRepStart = false;			// remember, we're dealing with an ASYNC ISR
    startMicros = micros();
    do {
      TWDR = mytwi_slarw;
      if((mytwi_timeout_us > 0ul) && ((micros() - startMicros) > mytwi_timeout_us)) {
        mytwi_handleTimeout(mytwi_do_reset_on_timeout);
        return (5);
      }
    } while(TWCR & _BV(TWWC));
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE);	// enable INTs, but not START
  } else {
    // send start condition
    TWCR = _BV(TWINT) | _BV(TWEA) | _BV(TWEN) | _BV(TWIE) | _BV(TWSTA);	// enable INTs
  }

  // wait for write operation to complete
  startMicros = micros();
  while(wait && (TWI_MTX == mytwi_state)){
    if((mytwi_timeout_us > 0ul) && ((micros() - startMicros) > mytwi_timeout_us)) {
      mytwi_handleTimeout(mytwi_do_reset_on_timeout);
      return (5);
    }
  }

  if (mytwi_error == 0xFF)
    return 0;	// success
  else if (mytwi_error == TW_MT_SLA_NACK)
    return 2;	// error: address send, nack received
  else if (mytwi_error == TW_MT_DATA_NACK)
    return 3;	// error: data send, nack received
  else
    return 4;	// other twi error
}

/*
 * Function twi_transmit
 * Desc     fills slave tx buffer with data
 *          must be called in slave tx event callback
 * Input    data: pointer to byte array
 *          length: number of bytes in array
 * Output   1 length too long for buffer
 *          2 not slave transmitter
 *          0 ok
 */
uint8_t mytwi_transmit(const uint8_t* data, uint8_t length)
{
  uint8_t i;

  // ensure data will fit into buffer
  if(TWI_BUFFER_LENGTH < (mytwi_txBufferLength+length)){
    return 1;
  }

  // ensure we are currently a slave transmitter
  if(TWI_STX != mytwi_state){
    return 2;
  }

  // set length and copy data into tx buffer
  for(i = 0; i < length; ++i){
    mytwi_txBuffer[mytwi_txBufferLength+i] = data[i];
  }
  mytwi_txBufferLength += length;

  return 0;
}

/*
 * Function twi_attachSlaveRxEvent
 * Desc     sets function called before a slave read operation
 * Input    function: callback function to use
 * Output   none
 */
void mytwi_attachSlaveRxEvent( void (*function)(uint8_t*, int) )
{
  mytwi_onSlaveReceive = function;
}

/*
 * Function twi_attachSlaveTxEvent
 * Desc     sets function called before a slave write operation
 * Input    function: callback function to use
 * Output   none
 */
void mytwi_attachSlaveTxEvent( void (*function)(void) )
{
  mytwi_onSlaveTransmit = function;
}

/*
 * Function twi_reply
 * Desc     sends byte or readys receive line
 * Input    ack: byte indicating to ack or to nack
 * Output   none
 */
void mytwi_reply(uint8_t ack)
{
  // transmit master read ready signal, with or without ack
  if(ack){
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT) | _BV(TWEA);
  }else{
    TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWINT);
  }
}

/*
 * Function twi_stop
 * Desc     relinquishes bus master status
 * Input    none
 * Output   none
 */
void mytwi_stop(void)
{
  // send stop condition
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT) | _BV(TWSTO);

  // wait for stop condition to be exectued on bus
  // TWINT is not set after a stop condition!
  // We cannot use micros() from an ISR, so approximate the timeout with cycle-counted delays
  const uint8_t us_per_loop = 8;
  uint32_t counter = (mytwi_timeout_us + us_per_loop - 1)/us_per_loop; // Round up
  while(TWCR & _BV(TWSTO)){
    if(mytwi_timeout_us > 0ul){
      if (counter > 0ul){
        _delay_us(10);
        counter--;
      } else {
        mytwi_handleTimeout(mytwi_do_reset_on_timeout);
        return;
      }
    }
  }

  // update twi state
  mytwi_state = TWI_READY;
}

/*
 * Function twi_releaseBus
 * Desc     releases bus control
 * Input    none
 * Output   none
 */
void mytwi_releaseBus(void)
{
  // release bus
  TWCR = _BV(TWEN) | _BV(TWIE) | _BV(TWEA) | _BV(TWINT);

  // update twi state
  mytwi_state = TWI_READY;
}

/*
 * Function twi_setTimeoutInMicros
 * Desc     set a timeout for while loops that twi might get stuck in
 * Input    timeout value in microseconds (0 means never time out)
 * Input    reset_with_timeout: true causes timeout events to reset twi
 * Output   none
 */
void mytwi_setTimeoutInMicros(uint32_t timeout, bool reset_with_timeout){
  mytwi_timed_out_flag = false;
  mytwi_timeout_us = timeout;
  mytwi_do_reset_on_timeout = reset_with_timeout;
}

/*
 * Function twi_handleTimeout
 * Desc     this gets called whenever a while loop here has lasted longer than
 *          twi_timeout_us microseconds. always sets twi_timed_out_flag
 * Input    reset: true causes this function to reset the twi hardware interface
 * Output   none
 */
void mytwi_handleTimeout(bool reset){
  mytwi_timed_out_flag = true;

  if (reset) {
    // remember bitrate and address settings
    uint8_t previous_TWBR = TWBR;
    uint8_t previous_TWAR = TWAR;

    // reset the interface
    mytwi_disable();
    mytwi_init();

    // reapply the previous register values
    TWAR = previous_TWAR;
    TWBR = previous_TWBR;
  }
}

/*
 * Function twi_manageTimeoutFlag
 * Desc     returns true if twi has seen a timeout
 *          optionally clears the timeout flag
 * Input    clear_flag: true if we should reset the hardware
 * Output   none
 */
bool mytwi_manageTimeoutFlag(bool clear_flag){
  bool flag = mytwi_timed_out_flag;
  if (clear_flag){
    mytwi_timed_out_flag = false;
  }
  return(flag);
}

ISR(TWI_vect)
{
  switch(TW_STATUS){
    // All Master
    case TW_START:     // sent start condition
    case TW_REP_START: // sent repeated start condition
      // copy device address and r/w bit to output register and ack
      TWDR = mytwi_slarw;
      mytwi_reply(1);
      break;

    // Master Transmitter
    case TW_MT_SLA_ACK:  // slave receiver acked address
    case TW_MT_DATA_ACK: // slave receiver acked data
      // if there is data to send, send it, otherwise stop
      if(mytwi_masterBufferIndex < mytwi_masterBufferLength){
        // copy data to output register and ack
        TWDR = mytwi_masterBuffer[mytwi_masterBufferIndex++];
        mytwi_reply(1);
      }else{
        if (mytwi_sendStop){
          mytwi_stop();
       } else {
         mytwi_inRepStart = true;	// we're gonna send the START
         // don't enable the interrupt. We'll generate the start, but we
         // avoid handling the interrupt until we're in the next transaction,
         // at the point where we would normally issue the start.
         TWCR = _BV(TWINT) | _BV(TWSTA)| _BV(TWEN) ;
         mytwi_state = TWI_READY;
        }
      }
      break;
    case TW_MT_SLA_NACK:  // address sent, nack received
      mytwi_error = TW_MT_SLA_NACK;
      mytwi_stop();
      break;
    case TW_MT_DATA_NACK: // data sent, nack received
      mytwi_error = TW_MT_DATA_NACK;
      mytwi_stop();
      break;
    case TW_MT_ARB_LOST: // lost bus arbitration
      mytwi_error = TW_MT_ARB_LOST;
      mytwi_releaseBus();
      break;

    // Master Receiver
    case TW_MR_DATA_ACK: // data received, ack sent
      // put byte into buffer
      mytwi_masterBuffer[mytwi_masterBufferIndex++] = TWDR;
      __attribute__ ((fallthrough));
    case TW_MR_SLA_ACK:  // address sent, ack received
      // ack if more bytes are expected, otherwise nack
      if(mytwi_masterBufferIndex < mytwi_masterBufferLength){
        mytwi_reply(1);
      }else{
        mytwi_reply(0);
      }
      break;
    case TW_MR_DATA_NACK: // data received, nack sent
      // put final byte into buffer
      mytwi_masterBuffer[mytwi_masterBufferIndex++] = TWDR;
      if (mytwi_sendStop){
        mytwi_stop();
      } else {
        mytwi_inRepStart = true;	// we're gonna send the START
        // don't enable the interrupt. We'll generate the start, but we
        // avoid handling the interrupt until we're in the next transaction,
        // at the point where we would normally issue the start.
        TWCR = _BV(TWINT) | _BV(TWSTA)| _BV(TWEN) ;
        mytwi_state = TWI_READY;
      }
      break;
    case TW_MR_SLA_NACK: // address sent, nack received
      mytwi_stop();
      break;
    // TW_MR_ARB_LOST handled by TW_MT_ARB_LOST case

    // Slave Receiver
    case TW_SR_SLA_ACK:   // addressed, returned ack
    case TW_SR_GCALL_ACK: // addressed generally, returned ack
    case TW_SR_ARB_LOST_SLA_ACK:   // lost arbitration, returned ack
    case TW_SR_ARB_LOST_GCALL_ACK: // lost arbitration, returned ack
      // enter slave receiver mode
      mytwi_state = TWI_SRX;
      // indicate that rx buffer can be overwritten and ack
      mytwi_rxBufferIndex = 0;
      mytwi_reply(1);
      break;
    case TW_SR_DATA_ACK:       // data received, returned ack
    case TW_SR_GCALL_DATA_ACK: // data received generally, returned ack
      // if there is still room in the rx buffer
      if(mytwi_rxBufferIndex < TWI_BUFFER_LENGTH){
        // put byte in buffer and ack
        mytwi_rxBuffer[mytwi_rxBufferIndex++] = TWDR;
        mytwi_reply(1);
      }else{
        // otherwise nack
        mytwi_reply(0);
      }
      break;
    case TW_SR_STOP: // stop or repeated start condition received
      // ack future responses and leave slave receiver state
      mytwi_releaseBus();
      // put a null char after data if there's room
      if(mytwi_rxBufferIndex < TWI_BUFFER_LENGTH){
        mytwi_rxBuffer[mytwi_rxBufferIndex] = '\0';
      }
      // callback to user defined callback
      mytwi_onSlaveReceive(mytwi_rxBuffer, mytwi_rxBufferIndex);
      // since we submit rx buffer to "wire" library, we can reset it
      mytwi_rxBufferIndex = 0;
      break;
    case TW_SR_DATA_NACK:       // data received, returned nack
    case TW_SR_GCALL_DATA_NACK: // data received generally, returned nack
      // nack back at master
      mytwi_reply(0);
      break;

    // Slave Transmitter
    case TW_ST_SLA_ACK:          // addressed, returned ack
    case TW_ST_ARB_LOST_SLA_ACK: // arbitration lost, returned ack
      // enter slave transmitter mode
      mytwi_state = TWI_STX;
      // ready the tx buffer index for iteration
      mytwi_txBufferIndex = 0;
      // set tx buffer length to be zero, to verify if user changes it
      mytwi_txBufferLength = 0;
      // request for txBuffer to be filled and length to be set
      // note: user must call twi_transmit(bytes, length) to do this
      mytwi_onSlaveTransmit();
      // if they didn't change buffer & length, initialize it
      if(0 == mytwi_txBufferLength){
        mytwi_txBufferLength = 1;
        mytwi_txBuffer[0] = 0x00;
      }
      __attribute__ ((fallthrough));
      // transmit first byte from buffer, fall
    case TW_ST_DATA_ACK: // byte sent, ack returned
      // copy data to output register
      TWDR = mytwi_txBuffer[mytwi_txBufferIndex++];
      // if there is more to send, ack, otherwise nack
      if(mytwi_txBufferIndex < mytwi_txBufferLength){
        mytwi_reply(1);
      }else{
        mytwi_reply(0);
      }
      break;
    case TW_ST_DATA_NACK: // received nack, we are done
    case TW_ST_LAST_DATA: // received ack, but we are done already!
      // ack future responses
      mytwi_reply(1);
      // leave slave receiver state
      mytwi_state = TWI_READY;
      break;

    // All
    case TW_NO_INFO:   // no state information
      break;
    case TW_BUS_ERROR: // bus error, illegal stop/start
      mytwi_error = TW_BUS_ERROR;
      mytwi_stop();
      break;
  }
}
