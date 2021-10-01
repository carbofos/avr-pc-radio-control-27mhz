#define RC_PORT_DDR        DDRB
#define RC_PORT_OUTPUT     PORTB
#define RC_BIT             1


#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>  /* for sei() */
#include <util/delay.h>     /* for _delay_ms() */

#include <avr/pgmspace.h>   /* required by usbdrv.h */
#include "usbdrv.h"
// #include "oddebug.h"        /* This is also an example for using debug macros */
#include "requests.h"       /* The custom request numbers we use */

PROGMEM const char usbHidReportDescriptor[22] = {   /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

/* ------------------------------------------------------------------------- */

static unsigned char rc_code = 10;
const static char header[] = { 1,1,1,0, 1,1,1,0, 1,1,1,0, 1,1,1,0 };
const static unsigned char headerlength = 16;
volatile unsigned char currentcode_position = 0;
static unsigned char currentcode = 0;

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
usbRequest_t    *rq = (void *)data;
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_VENDOR){
        if(rq->bRequest == CUSTOM_RQ_SET_STATUS){
            rc_code = rq->wValue.bytes[0];
        }else if(rq->bRequest == CUSTOM_RQ_GET_STATUS){
            static uchar dataBuffer[1];     /* buffer must stay valid when usbFunctionSetup returns */
            dataBuffer[0] = rc_code;
            usbMsgPtr = (usbMsgPtr_t) dataBuffer;         /* tell the driver which data to return */
            return 1;                       /* tell the driver to send 1 byte */
        }
    }
    return 0;
}

// This function should be called every 500us (microseconds)
ISR (TIMER0_COMPA_vect)
{
    if (currentcode_position == 0)
        currentcode = rc_code;

    unsigned char currentcodelength = headerlength + (currentcode * 2);

    if (currentcode > 0)
    {
        if(currentcode_position < headerlength)
        {
            if (header[currentcode_position] == 1)
                RC_PORT_OUTPUT |= ( 1 << RC_BIT);
            else
                RC_PORT_OUTPUT &= ~(1 << RC_BIT);
        }
        else
        {
            if ( (currentcode_position - headerlength ) % 2 )
                RC_PORT_OUTPUT &= ~(1 << RC_BIT);
            else
                RC_PORT_OUTPUT |= ( 1 << RC_BIT);
        }
    }

    ++currentcode_position;
    if (currentcode_position >= currentcodelength)
        currentcode_position = 0;

}

void init_timer()
{
    cli();
    TCCR0A = (1 << WGM01 )| (0 << WGM00 );
    TCCR0B = (1 << CS02)|(0 << CS01)|(1 << CS00);      // clock source CLK/1024, start timer
    OCR0A  = 4;
    TCNT0 = 0;
    TIMSK = 1 << OCIE0A;       // compare match A interrupt enable
    sei();
}

int __attribute__((noreturn)) main(void)
{
uchar   i;
    wdt_enable(WDTO_1S);
    // odDebugInit();
    usbInit();
    // cli();
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
    usbDeviceConnect();
    sei();

    RC_PORT_DDR |= (1 << RC_BIT); // output
    init_timer();

    for(;;){
        wdt_reset();
        usbPoll();
    }
}

