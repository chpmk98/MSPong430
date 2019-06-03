#include <msp430.h> 
#include "msprf24.h"
#include "nrf_userconfig.h"
#include <stdint.h>

volatile unsigned int user;
/**
 * main.c
 */
int main(void)
{
    uint8_t addr[5];
    uint8_t buf[32];
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
    // Configure one FRAM waitstate as required by the device datasheet for MCLK
    // operation beyond 8MHz _before_ configuring the clock system.
    /*FRCTL0 = FRCTLPW | NWAITS_1;

    // Clock System Setup
    CSCTL0_H = CSKEY >> 8;                    // Unlock CS registers
    CSCTL1 = DCORSEL | DCOFSEL_4;             // Set DCO to 16MHz
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all dividers

    CSCTL0_H = 0;                             // Lock CS registerss*/
	PM5CTL0 &= ~LOCKLPM5;

	__bis_SR_register(GIE); // enable interrupt
	
	P4DIR |= BIT2;
    P1DIR |= BIT1 + BIT0;
    P1OUT &= ~(BIT1 + BIT0);

	rf_crc = RF24_EN_CRC | RF24_CRCO;
	rf_addr_width = 5;
	rf_speed_power = RF24_SPEED_1MBPS | RF24_POWER_0DBM;
	rf_channel = 120;

	 msprf24_init();  // All RX pipes closed by default
	 msprf24_set_pipe_packetsize(0, 32);
     msprf24_open_pipe(0, 0);  // Open pipe#0 with Enhanced ShockBurst enabled for receiving Auto-ACKs

	    // Transmit to 'rad01' (0x72 0x61 0x64 0x30 0x31)
    msprf24_standby();
    user = msprf24_current_state();
    addr[0] = 0xBA; addr[1] = 0xDB; addr[2] = 0xED; addr[3] = 0xFA; addr[4] = 0xCE;
    w_tx_addr(addr);
    w_rx_addr(0, addr);  // Pipe 0 receives auto-ack's, autoacks are sent back to the TX addr so the PTX node
	                     // needs to listen to the TX addr on pipe#0 to receive them.

    // Receive mode

    //if (!(RF24_QUEUE_RXEMPTY & msprf24_queue_state())) {
    //    flush_rx();
   // }
   // msprf24_activate_rx();
   // msprf24_standby();
    int i;
    for(i = 0; i < 32;i++){
        buf[i] = i;
    }
    msprf24_activate_rx();
    //w_tx_payload(32, buf);
    //msprf24_activate_tx();
    while(1){
        P4OUT |= BIT2;
        //__delay_cycles(1000000);

        //LPM4;

        if (rf_irq & RF24_IRQ_FLAGGED) {
            msprf24_get_irq_reason();
            if (rf_irq & RF24_IRQ_TX){
                P1OUT &= ~BIT0; // Red LED off
                P1OUT |= BIT1;  // Green LED on
                msprf24_irq_clear(rf_irq);
                //__delay_cycles(1000000);
                //msprf24_close_pipe(0);
                //msprf24_open_pipe(0,0);
                // restart this bad boy
                msprf24_powerdown();
                msprf24_init();  // All RX pipes closed by default
                msprf24_set_pipe_packetsize(0, 32);
                msprf24_open_pipe(0, 0);
                msprf24_standby();

                w_tx_addr(addr);
                w_rx_addr(0, addr);
                msprf24_activate_rx();
                //continue;
            }
            if (rf_irq & RF24_IRQ_TXFAILED){
                P1OUT &= ~BIT1; // Green LED off
                //P1OUT |= BIT0;  // Red LED on
                msprf24_irq_clear(rf_irq);
            }
            if (rf_irq & RF24_IRQ_RX) {
                r_rx_payload(32, buf);
                __no_operation();
                for(i = 0; i < 32;i++){
                    buf[i]++;
                }
                P1OUT |= BIT0;
                P1OUT &= ~BIT1;
                msprf24_irq_clear(rf_irq);
                //__delay_cycles(1000000);
                //msprf24_close_pipe(0);
                // restart this bad boy
                msprf24_powerdown();
                msprf24_init();  // All RX pipes closed by default
                msprf24_set_pipe_packetsize(0, 32);
                msprf24_open_pipe(0, 0);
                msprf24_standby();
                //msprf24_open_pipe(0,0);
                w_tx_addr(addr);
                w_rx_addr(0, addr);
                __delay_cycles(800000);
                w_tx_payload(32, buf);
                msprf24_activate_tx();
            }
            //user = msprf24_get_last_retransmits();
        }
    }
	//return 0;
}
