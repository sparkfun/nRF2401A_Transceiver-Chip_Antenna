/*
    5-30-04
    Copyright Spark Fun Electronics© 2004
    
    RF-24G Configuration and testing. The 24G requires 500ns between Data Setup and Clk, so we ran this on a 16F88 at
    internal 8MHz which turns into 500ns per instruction. Imagine a breadboard with a 16F88 connected to two transceivers
    inserted into the same breadboard about 4 inches apart. This made it easy for testing the setup on the units and 
    proof of transmission, but not a good setup for testing the effective communication distance.
    
    The RF-24G requires 3V!! No 5V! So we ran our 16F88 (not 16LF88) at 3V and at 8MHz. This is out of spec for both minimum 
    voltage (4V) and maximum frequency at 3V (4MHz) but it worked great! Of course it shouldn't be used for a deployed design.    
    
    The time delay between clocking in the next data is given by the equation on page 31.
    Time On Air = (databits+1) / datarate  
    T(OA) = 266 bits (max) / 1,000,000 bps = 266us
    
    NOTE: If you enable the receiver (set CE high), the receiver will start monitoring the air. With the CRC
    set to 8 bit (default) the receiver will find all sorts of junk in the air with a correct CRC tag. Our recommendation
    is to either transmit a resonably constant stream of data, use 16-bit CRC, and/or use additional header/end bytes in
    the payload to verify incoming packets.
    

    config_setup word 16 bits found on pages 13-15
    
    23: 0 Payloads have an 8 bit address
    22: 0
    21: 1
    20: 0
    19: 0
    18: 0
    17: 1 16-Bit CRC
    16: 1 CRC Enabled

    15: 0 One channel receive
    14: 1 ShockBurst Mode
    13: 1 1Mbps Transmission Rate
    12: 0
    11: 1
    10: 1
    9: 1 RF Output Power
    8: 0 RF Output Power

    7: 0 Channel select (channel 2)
    6: 0
    5: 0
    4: 0
    3: 0
    2: 1
    1: 0
    0: 0 Transmit mode
    
*/
#define Clock_8MHz
#define Baud_9600

#include "d:\Pics\c\16F88.h"

//There is no config word because this program tested on a 16F88 using Bloader the boot load program

#pragma origin 4

#include "d:\Pics\code\Delay.c"     // Delays
#include "d:\Pics\code\Stdio.c"     // Basic Serial IO

#define TX_CE      PORTB.0
#define TX_CS      PORTB.1
#define TX_CLK1    PORTB.3
#define TX_DATA    PORTB.4

#define RX_CE       PORTA.2
#define RX_CS       PORTA.3
#define RX_CLK1     PORTA.4
#define RX_DATA     PORTA.1
#define RX_DR       PORTA.0

uns8 data_array[4];
uns8 counter;

void boot_up(void);
void configure_receiver(void);
void configure_transmitter(void);
void transmit_data(void);
void receive_data(void);

void main()
{
    uns16 elapsed_time;

    counter = 0;
    
    boot_up();

    while(1)
    {
        counter++;
        
        data_array[0] = 0x12;
        data_array[1] = 0x34;
        data_array[2] = 0xAB;
        data_array[3] = counter;

        printf("\n\rSending data...\n\r", 0);
        transmit_data();
    
        //Here we monitor how many clock cycles it takes for the receiver to register good data
        //elasped_time is in cycles - each cycles is 500ns at 8MHz so 541 cycles = 270.5us
        //==============================================
        TMR1IF = 0;
        TMR1L = 0 ; TMR1H = 0 ; TMR1ON = 1;
        while(RX_DR == 0)
            if (TMR1IF == 1) break; //If timer1 rolls over waiting for data, then break
        TMR1ON = 0;
        elapsed_time.high8 = TMR1H;
        elapsed_time.low8 = TMR1L;
        printf("Time to receive = %d\n\r", elapsed_time);
        //==============================================
        
        if(RX_DR == 1) //We have data!
            receive_data();
        else
            printf("No data found!\n\r", 0);
        
        delay_ms(1000); //Have a second between transmissions just for evaluation
    
    }
        
}

void boot_up(void)
{
    OSCCON = 0b.0111.0000; //Setup internal oscillator for 8MHz
    while(OSCCON.2 == 0); //Wait for frequency to stabilize

    ANSEL = 0b.0000.0000; //Turn pins to Digital instead of Analog
    CMCON = 0b.0000.0111; //Turn off comparator on RA port

    PORTA = 0b.0000.0000;  
    TRISA = 0b.0000.0001;  //0 = Output, 1 = Input (RX_DR is on RA0)

    PORTB = 0b.0000.0000;  
    TRISB = 0b.0000.0100;  //0 = Output, 1 = Input (RX is an input)

    enable_uart_TX(0); //Setup the hardware UART for 20MHz at 9600bps
    enable_uart_RX(0); //Take a look at header files - it's not that hard to setup the UART
    
    printf("\n\rRF-24G Testing:\n\r", 0);
    
    delay_ms(100);

    configure_transmitter();
    configure_receiver();

}

//This will clock out the current payload into the data_array
void receive_data(void)
{
    uns8 i, j, temp;

    RX_CE = 0;//Power down RF Front end

    //Erase the current data array so that we know we are looking at actual received data
    data_array[0] = 0x00;
    data_array[1] = 0x00;
    data_array[2] = 0x00;
    data_array[3] = 0x00;

    //Clock in data, we are setup for 32-bit payloads
    for(i = 0 ; i < 4 ; i++) //4 bytes
    {
        for(j = 0 ; j < 8 ; j++) //8 bits each
        {
            temp <<= 1;
            temp.0 = RX_DATA;

            RX_CLK1 = 1;
            RX_CLK1 = 0;
        }

        data_array[i] = temp; //Store this byte
    }
    
    if(RX_DR == 0) //Once the data is clocked completely, the receiver should make DR go low
        printf("DR went low\n\r", 0);
    
    printf("\n\rData Received:\n\r", 0);
    printf("[0] : %h\n\r", data_array[0]);
    printf("[1] : %h\n\r", data_array[1]);
    printf("[2] : %h\n\r", data_array[2]);
    printf("[3] : %h\n\r", data_array[3]);

    RX_CE = 1; //Power up RF Front end
}

//This sends out the data stored in the data_array
//data_array must be setup before calling this function
void transmit_data(void)
{
    uns8 i, j, temp, rf_address;

    TX_CE = 1;

    //Clock in address
    rf_address = 0b.1110.0111; //Power-on Default for all units (on page 11)
    for(i = 0 ; i < 8 ; i++)
    {
        TX_DATA = rf_address.7;
        TX_CLK1 = 1;
        TX_CLK1 = 0;
        
        rf_address <<= 1;
    }
    
    //Clock in the data_array
    for(i = 0 ; i < 4 ; i++) //4 bytes
    {
        temp = data_array[i];
        
        for(j = 0 ; j < 8 ; j++) //One bit at a time
        {
            TX_DATA = temp.7;
            TX_CLK1 = 1;
            TX_CLK1 = 0;
            
            temp <<= 1;
        }
    }
    
    TX_CE = 0; //Start transmission   
}

//2.4G Configuration - Receiver
//This setups up a RF-24G for receiving at 1mbps
void configure_receiver(void)
{
    uns8 i;
    uns24 config_setup;

    //During configuration of the receiver, we need RX_DATA as an output
    PORTA = 0b.0000.0000;  
    TRISA = 0b.0000.0001;  //0 = Output, 1 = Input (RX_DR is on RA0) (RX_DATA is on RA1)

    //Config Mode
    RX_CE = 0; RX_CS = 1;
    
    //Delay of 5us from CS to Data (page 30) is taken care of by the for loop
    
    //Setup configuration word
    config_setup = 0b.0010.0011.0110.1110.0000.0101; //Look at pages 13-15 for more bit info

    for(i = 0 ; i < 24 ; i++)
    {
        RX_DATA = config_setup.23;
        RX_CLK1 = 1;
        RX_CLK1 = 0;
        
        config_setup <<= 1;
    }
    
    //Configuration is actived on falling edge of CS (page 10)
    RX_CE = 0; RX_CS = 0;

    //After configuration of the receiver, we need RX_DATA as an input
    PORTA = 0b.0000.0000;  
    TRISA = 0b.0000.0011;  //0 = Output, 1 = Input (RX_DR is on RA0) (RX_DATA is on RA1)

    //Start monitoring the air
    RX_CE = 1; RX_CS = 0;

    printf("RX Configuration finished...\n\r", 0);

}    

//2.4G Configuration - Transmitter
//This sets up one RF-24G for shockburst transmission
void configure_transmitter(void)
{
    uns8 i;
    uns24 config_setup;

    //Config Mode
    TX_CE = 0; TX_CS = 1;
    
    //Delay of 5us from CS to Data (page 30) is taken care of by the for loop
    
    //Setup configuration word
    config_setup = 0b.0010.0011.0110.1110.0000.0100; //Look at pages 13-15 for more bit info

    for(i = 0 ; i < 24 ; i++)
    {
        TX_DATA = config_setup.23;
        TX_CLK1 = 1;
        TX_CLK1 = 0;
        
        config_setup <<= 1;
    }
    
    //Configuration is actived on falling edge of CS (page 10)
    TX_CE = 0; TX_CS = 0;

    printf("TX Configuration finished...\n\r", 0);
}

