/* --COPYRIGHT--,BSD
 * Copyright (c) 2016, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
//******************************************************************************
#include <stdint.h>
#include <msp430.h>
#include <string.h>
#include "CTS_Layer.h"
#include "msprf24.h"
#include "nrf_userconfig.h"

//******************************************************************************
//UART printf definitions
//******************************************************************************

void init_uart() {
    // Pins, 2.1/UCA0RXD (to MCU), 2.0/UCA0TXD (from MCU)
    P2REN = 0;
    P2SEL1 |= (BIT1 | BIT0);
    P2SEL0 &= ~(BIT1 | BIT0);

    // Configure USCI_A0 for UART mode
    UCA0CTLW0 = UCSWRST;                    // Put eUSCI in reset
    UCA0CTLW0 |= UCSSEL_2;             // CLK = SMCLK
    // Baud Rate calculation
    // 8000000/(16*9600) = 52.083
    // Fractional portion = 0.083
    // User's Guide Table 21-4: UCBRSx = 0x04
    // UCBRFx = int ( (52.083-52)*16) = 1
    UCA0BRW = 52/2;                           // 8000000/16/9600
    //UCA0BR1 = 0x00;
    UCA0MCTLW |= UCOS16 | UCBRF_1 | 0x4900;
    UCA0CTLW0 &= ~UCSWRST;                  // Initialize eUSCI
}

void printf(char *, ...);

// The I/O backend library
int io_putchar(int c) {
    while (!(UCA0IFG&UCTXIFG));                // USCI_A0 TX buffer ready?
    UCA0TXBUF = c;
    return 0;
}

int io_puts_no_newline(const char *str) {
  uint8_t len_str = strlen(str);
  volatile uint8_t i;
  for(i=0;i<len_str;i++) {
    while (!(UCA0IFG&UCTXIFG));                // USCI_A0 TX buffer ready?
    UCA0TXBUF = str[i];
  }
  return 0;
}

//******************************************************************************
//Game definitions
//******************************************************************************
#define MAX_X 500
#define MAX_Y 500
#define PADDLESPEED 20
#define PADDLEWIDTH 200
#define PADDLEHEIGHT 20
struct Sprite {
    float px;
    float py;
    float vx;
    float vy;
};

struct Sprite paddle;
struct Sprite ball;
uint32_t hasBall; // do you have the ball?
uint32_t ballIsLive; //is the ball moving?
int myScore;
int theirScore;

// radio vars
uint8_t addr[5];
uint8_t buf[32];
struct bufStruct {
    int myScore;
    int theirScore;
    float px;
    float py;
    float vx;
    float vy;
    uint32_t hasBall;
};
struct bufStruct* bufBoi = buf;

void initGame(){
    paddle.px = MAX_X / 2;
    paddle.py = 400;
    ball.px = MAX_X / 2;
    ball.py = MAX_Y / 2;
    ball.vx = 0;
    ball.vy = 0;
}
//check for ball collisions
//
void gameRules(){
    // if the ball is live and we don't have it,
    // listen until we get it
    if(!hasBall) {
        // see if the radio is intruiged
        if (rf_irq & RF24_IRQ_FLAGGED) {
            msprf24_get_irq_reason();
            // if we recieved a thing, update game state
            if (rf_irq & RF24_IRQ_RX) {
                r_rx_payload(32, buf);

                // update the position and velocity of the ball
                // as well as game information
                myScore = bufBoi->myScore;
                theirScore = bufBoi->theirScore;
                ball.px = bufBoi->px;
                ball.py = bufBoi->py;
                ball.vx = bufBoi->vx;
                ball.vy = bufBoi->vy;
                hasBall = bufBoi->hasBall;
                msprf24_irq_clear(rf_irq);

                // confirm that the game is in play
                ballIsLive = 1;
            } else {
                // otherwise, it's not important
                msprf24_irq_clear(rf_irq);
            }
        }
    }

    // if the ball is live and we have the ball on screen,
    // increment physics
    if(ballIsLive && hasBall){
        ball.px += ball.vx;
        ball.py += ball.vy;

        if(ball.px < 0){
            ball.px = 0;
            ball.vx = -ball.vx;
        }
        else if(ball.px >= MAX_X){
            ball.px = MAX_X;
            ball.vx = -ball.vx;
        }
        if(ball.py < 0){
            // once the ball reaches the top of the screen,
            // send a packet
            msprf24_powerdown();
            msprf24_init();  // All RX pipes closed by default
            msprf24_set_pipe_packetsize(0, 32);
            msprf24_open_pipe(0, 0);
            msprf24_standby();
            w_tx_addr(addr);
            w_rx_addr(0, addr);
            // send game information to other player
            bufBoi->myScore = theirScore;
            bufBoi->theirScore = myScore;
            bufBoi->px = MAX_X - ball.px;
            bufBoi->py = -ball.py;
            bufBoi->vx = -ball.vx;
            bufBoi->vy = -ball.vy;
            bufBoi->hasBall = 1;
            w_tx_payload(32, buf);
            msprf24_activate_tx();

            while(1) {
                // see if the radio is intruiged
                if (rf_irq & RF24_IRQ_FLAGGED) {
                    msprf24_get_irq_reason();
                    // if we sent a thing, swich to receiving
                    if (rf_irq & RF24_IRQ_TX) {
                        // then switch back to RX mode to listen for when the ball
                        // returns
                        msprf24_irq_clear(rf_irq);
                        msprf24_powerdown();
                        msprf24_init();  // All RX pipes closed by default
                        msprf24_set_pipe_packetsize(0, 32);
                        msprf24_open_pipe(0, 0);
                        msprf24_standby();

                        w_tx_addr(addr);
                        w_rx_addr(0, addr);
                        msprf24_activate_rx();

                        break;
                    } else {
                        msprf24_irq_clear(rf_irq);
                    }
                }
            }

            // note that we no longer have the ball
            hasBall = 0;
        }
        if(ball.py > MAX_Y){
            // if the ball hit the bottom of the screen, increment score
            // and send a transmission. start again with the ball.
            msprf24_powerdown();
            msprf24_init();  // All RX pipes closed by default
            msprf24_set_pipe_packetsize(0, 32);
            msprf24_open_pipe(0, 0);
            msprf24_standby();
            w_tx_addr(addr);
            w_rx_addr(0, addr);
            theirScore++;
            bufBoi->myScore = theirScore;
            bufBoi->theirScore = myScore;
            bufBoi->hasBall = 0;

            w_tx_payload(32, buf);
            msprf24_activate_tx();

            while(1) {
                // see if the radio is intruiged
                if (rf_irq & RF24_IRQ_FLAGGED) {
                    msprf24_get_irq_reason();
                    // if we sent a thing, swich to receiving
                    if (rf_irq & RF24_IRQ_TX) {
                        // then switch back to RX mode to listen for when the ball
                        // returns
                        msprf24_irq_clear(rf_irq);
                        msprf24_powerdown();
                        msprf24_init();  // All RX pipes closed by default
                        msprf24_set_pipe_packetsize(0, 32);
                        msprf24_open_pipe(0, 0);
                        msprf24_standby();

                        w_tx_addr(addr);
                        w_rx_addr(0, addr);
                        msprf24_activate_rx();

                        break;
                    } else {
                        msprf24_irq_clear(rf_irq);
                    }
                }
            }

            initGame();
            CEINT |= CEIE;  // enable interrupts again
        }
    }

    // check for collision with the paddle and update
    if(ball.px >= (paddle.px - PADDLEWIDTH / 2) && ball.px <= (paddle.px + PADDLEWIDTH/2)){
        if(ball.py >= (paddle.py - PADDLEHEIGHT / 2) && ball.py <= (paddle.py + PADDLEHEIGHT /2)){
            if(ball.vy < 0)
                return;
            ball.vy = -ball.vy;
            //spice things up a bit of risk
            float risk = (float)abs(ball.px - paddle.px) / (PADDLEWIDTH/2);
            float newvx = ball.vx;
            float newvy = ball.vy;
            //your speed will be altered based on your risk
            newvx *= (0.8 +  risk * risk);
            newvy *= (0.8 +  risk * risk);
            ball.vx = newvx;
            ball.vy = newvy;
            //don't let vx or vy get too slow or too fast
            if(ball.vy > -5)
                ball.vy = -5;
            if(ball.vy < -19)
                ball.vy = -19;
            if(ball.vx < 0 && ball.vx > -5)
                ball.vx = -5;
            if(ball.vx < 0 && ball.vx < -15)
                ball.vx = -15;
            if(ball.vx > 0 && ball.vx > 5)
                ball.vx = 5;
            if(ball.vx > 0 && ball.vx < 15)
                ball.vx = 15;
        }
    }
}

//******************************************************************************
//Sleep Definitions
//******************************************************************************
// Sleep Function
// Configures Timer A to run off ACLK, count in UP mode, places the CPU in LPM3 
// and enables the interrupt vector to jump to ISR upon timeout 
#define DELAY 100       // Timer delay timeout count, 5000*0.1msec = 500 msec
void sleep(unsigned int time)
{
    TA0CCR0 = time;
    TA0CTL = TASSEL_1+MC_1+TACLR;
    TA0CCTL0 &= ~CCIFG;
    TA0CCTL0 |= CCIE;
    __bis_SR_register(LPM3_bits+GIE);
    __no_operation();
}

void initCaptouch(){
    // Initialize Baseline measurement
    TI_CAPT_Init_Baseline(&wheel_buttons);
    // Update baseline measurement (Average 5 measurements)
    TI_CAPT_Update_Baseline(&wheel_buttons,5);
}

//******************************************************************************
//CapTouch Definitions
//******************************************************************************

struct Element * keyPressed;            // Pointer to the Element structure
void checkKeyPress(){
    // Return the pointer to the element which has been touched
    keyPressed = (struct Element *) TI_CAPT_Buttons(&wheel_buttons);

    // If a button has been touched, then take some action
    if (keyPressed)
    {
        //handle slider presses
        if (keyPressed == &SL1)
        {
            P4OUT |= BIT1;
            paddle.px -= PADDLESPEED;
        }
        else{
            P4OUT &= ~BIT1;
        }
        if (keyPressed == &SL2)
        {
            P4OUT |= BIT2;
            paddle.px -= PADDLESPEED/2;
        }
        else{
            P4OUT &= ~BIT2;
        }
        if (keyPressed == &SL4)
        {
            P4OUT |= BIT3;
            paddle.px += PADDLESPEED/2;
        }
        else{
            P4OUT &= ~BIT3;
        }
        if (keyPressed == &SL5)
        {
            P1OUT |= BIT1;
            paddle.px += PADDLESPEED;
        }
        else{
            P1OUT &= ~BIT1;
        }
        if(paddle.px <= 0)
            paddle.px = 0;
        if(paddle.px >= MAX_X)
            paddle.px = MAX_X;
        //handle button presses
        if (keyPressed == &B1)
        {
            P1OUT |= BIT0;
            if(hasBall){
                ballIsLive = 1;
            }
        }
        else{
            P1OUT &= ~BIT0;
        }
    }
    else
    {
        P1OUT &= ~BIT0 & ~BIT1;
        P4OUT &= ~BIT3 & ~BIT2 & ~BIT1;                // Turn off LEDs
    }
}

//******************************************************************************
//Clock definitions
//******************************************************************************
void init_clocks(){
    // Startup clock system with max DCO setting ~8MHz
    CSCTL0_H = CSKEY_H;                     // Unlock CS registers
    CSCTL1 = DCOFSEL_3 | DCORSEL;           // Set DCO to 8MHz
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;   // Set all dividers
    CSCTL0_H = 0;                           // Lock CS registers
}

//******************************************************************************
//Comparator Definitions
//******************************************************************************


void init_compe(){
    // Setup Comparator_E
    CECTL0 = CEIPEN | CEIPSEL_12;           // Enable V+, input channel CE12
    CECTL1 = CEPWRMD_1;                     // normal power mode
    CEINT |= CEIE;                          // enable interrupts
    CEINT &= ~CEIFG;                        // clear interrupt flag for good measure
    CECTL2 = CEREFL_2 | CERS_3 | CERSEL;    // VREF is applied to -terminal
                                            // R-ladder off; bandgap ref voltage
                                            // supplied to ref amplifier to get Vcref=2.0V
    CECTL3 = CEPD12;                        // Input Buffer Disable @P3.0/CE12, see section 35.2.7
    CECTL1 |= CEON;                         // Turn On Comparator_E (finally)
}



//******************************************************************************
//******************************************************************************
//The Big Bang
//******************************************************************************
//******************************************************************************
void main(void)
{ 
    WDTCTL = WDTPW + WDTHOLD;             // Stop watchdog timer

    // disable GPIO default high-impedance
    PM5CTL0 &= ~LOCKLPM5;

    init_clocks();

    //LED PIN INITIALIZATION
    P4DIR |= BIT3 | BIT2 | BIT1;// Set the pin controlling the center LED P4.2  as output pin
    P1DIR |= BIT0 | BIT1;
    P3DIR = 0xFF;		// Configure all Port 3 pins outputs
    P5DIR = BIT7;		// Configure all Port 5 pins outputs

    init_compe();
    init_uart();
    initCaptouch();

    // radio stuff
    //user = 0xFE;

    /* Initial values for nRF24L01+ library config variables */
    rf_crc = RF24_EN_CRC | RF24_CRCO; // CRC enabled, 16-bit
    rf_addr_width      = 5;
    rf_speed_power     = RF24_SPEED_1MBPS | RF24_POWER_0DBM;
    rf_channel     = 120;

    msprf24_init();
    msprf24_set_pipe_packetsize(0, 32);
    msprf24_open_pipe(0, 0);  // Open pipe#0 with Enhanced ShockBurst

    // some transmission stuff
    msprf24_standby();
    //user = msprf24_current_state();

    // Set our RX address
    addr[0] = 0xBA; addr[1] = 0xDB; addr[2] = 0xED; addr[3] = 0xFA; addr[4] = 0xCE;
    w_tx_addr(addr);
    w_rx_addr(0, addr);

    // Receive mode
    if (!(RF24_QUEUE_RXEMPTY & msprf24_queue_state())) {
        flush_rx();
    }

    // both players start in RX mode
    msprf24_activate_rx();

    initGame();

    // Main loop starts here
    while (1)
    {
        //check to see if key was pressed
        checkKeyPress();
        gameRules();

        printf("%u %u %u %u %u %u %u\r\n", (int)paddle.px, (int)paddle.py, (int)ball.px, (int)ball.py,hasBall, myScore, theirScore);
        // Put the MSP430 into LPM3 for a certain DELAY period
        sleep(DELAY);

    }
} // End Main

/******************************************************************************/
// Timer0_A0 Interrupt Service Routine: Disables the timer and exists LPM3
/******************************************************************************/
#pragma vector=TIMER0_A0_VECTOR
__interrupt void ISR_Timer0_A0(void)
{
  TA0CTL &= ~(MC_1);
  TA0CCTL0 &= ~(CCIE);
  __bic_SR_register_on_exit(LPM3_bits+GIE);
}
//comparator starts the game
#pragma vector = COMP_E_VECTOR
__interrupt void startGame(void){
    CEINT &= ~CEIFG; //reset interrupt flag
    CEINT &= ~CEIE;  // disable interrupts from now on
    hasBall = 1;
    ball.vx = -5 + rand() % (10+1);
    ball.vy = 5;
}
/*
#pragma vector=RTC_VECTOR,PORT2_VECTOR,TIMER2_A0_VECTOR,TIMER3_A0_VECTOR,    \
  USCI_A1_VECTOR,PORT1_VECTOR,TIMER1_A1_VECTOR,DMA_VECTOR,PORT3_VECTOR,      \
  TIMER0_A1_VECTOR,TIMER2_A1_VECTOR, TIMER3_A1_VECTOR,TIMER1_A0_VECTOR,      \
  USCI_B0_VECTOR,USCI_A0_VECTOR,TIMER0_B1_VECTOR,TIMER0_B0_VECTOR,           \
  UNMI_VECTOR,SYSNMI_VECTOR,AES256_VECTOR,PORT4_VECTOR,                      \
  ADC12_VECTOR
__interrupt void ISR_trap(void)
{
  // the following will cause an access violation which results in a SW BOR
  PMMCTL0 = PMMPW | PMMSWBOR;
}*/
