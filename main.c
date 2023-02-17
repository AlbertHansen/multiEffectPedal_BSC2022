#include "stdio.h"
#include "csl_intc.h"       // Interrupts
#include "csl_gpio.h"       // GPIO
#include "csl_sar.h"        // GPAIN
#include "ezdsp5535.h"      // board header
#include "ezdsp5535_gpio.h" // GPIO header
#include "aic3204.h"        // codec header
#include "cmath"            // for exponents
#include "stdlib.h"         // for abs(); and labs();

Int16 r;     // store read-values
Uint16 l;

///////////// Declare functions ////////////////////////////
// Init functions
void enableGPAINs();                        // enable GPAIN for updateVariables
void enableGPIOs();                         // enable GPIOs for updateVariables
void enableInterrupt();                     // enable GPIO interrupt
void pedal_init();                          // init board and codec
void AIC3204_init_44100Hz();                // setup for AIC to run fs = 44100 Hz

// Interrupt functions
extern void VECSTART(void);                 // Reference the start of the interrupt vector table
void updateVariables();                     // ISR, updates variables used in overdrive and reverb

// Read/write functions
void aic3204_codec_read_mod(Int16* input);  // read codec
void aic3204_codec_write_mod(Int16 output); // write codec

// Guitar effects
Int16 overdrive(Int16* overdrive_in);       // overdrive (returns 0, 1, 2 based on which section is used)
void reverb();                              // reverb
void updatePos();                           // update array position, includes wraparound
void clearArrays();                         // set all array-entries to zero

/////////// Necessary objects //////////////////////////////
GPIO_Handle GPIOHandle;         // handle for GPIO setup and interrupt-clear
CSL_Status  status;             // status (CSL_SOK)
Uint16 updateVariables_temp;    // holds value from SAR_read
Uint16 overdrive_status;        // on/off
Uint16 reverb_status;           // on/off

////////////////// overdrive setup  ////////////////////////
Uint16 linear_gain = 1.45 * 16384; // Q2.14

Int32 xx, axx, bx, c;     // for inbetween-calculations of -ax^2+bx-c
Int16 a   = 910;          // 1.7778 i Q7.9  format
Int16 b   = 1653;         // 3.2278 i Q7.9  format
Int16 c16 = 228;          // 0.4444 i Q7.9  format
Int16 gain = 16384;       // 2.0000 i Q3.13 format

Int16 lambda = 0.5*32768; // 0.5000 i Q1.15 format
Int16 gamma  = 0.8*32768; // 0.8000 i Q1.15 format

////////////////// reverb setup  ///////////////////////////
// variables
Int16 alpha       = 0.3 * 32768;      // dry/wet mix, Q1.15
Uint16 pre_delay  = 0;                // pre-delay, [samples]
double decay_time = 1;                // decay-time, [seconds]

Uint16 g          = 0.7  * 32768;     // Q1.15
Uint16 g_mod      = 0.51 * 32768;     // Q1.15

// arrays for previous values
Int16 x[8820+1];    // x[n]
Int16 C1[1323+1];   // comb 1
Int16 C2[1456+1];   // comb 2
Int16 C3[1808+1];   // comb 3
Int16 C4[1984+1];   // comb 4
Int32 C_sum[221+1]; // sum of comb outputs
Int16 v1[221+1];    // inner A1
Int32 A1[75+1];     // outer A1
Int16 v2[75+1];     // inner A2
Int32 A2;           // outer A2

// positions
Uint16 pos[7][2] = { // current position, delay position
  {0, 0}, // x
  {0, 1}, // C1
  {0, 1}, // C2
  {0, 1}, // C3
  {0, 1}, // C4
  {0, 1}, // C_sum and v1
  {0, 1}  // A1 and v2
};

// filter coefficients
Int16 a1 = 0.8130*32768;    // gain comb 1
Int16 a2 = 0.7961*32768;    // gain comb 2
Int16 a3 = 0.7534*32768;    // gain comb 3
Int16 a4 = 0.7329*32768;    // gain comb 4

////////////////// main ////////////////////////////////
int main(void){
    // setup
    pedal_init();       // init board and codec
    clearArrays();      // clear spurious values from arrays
    l = 0;  r = 0;      // init variables at 0 V

    GPIO_clearInt(GPIOHandle, CSL_GPIO_PIN15);
    // updateVariables();


    // main loop

    while(1){
        aic3204_codec_read_mod( &x[(pos[0][0])] );  // read analog value
        // GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_HIGH); // 96 clock-cycles??
        //CSL_GPIO_REGS->IOOUTDATA1 = CSL_GPIO_REGS->IOOUTDATA1 | (CSL_GPIO_IOOUTDATA1_OUT4_SET << CSL_GPIO_IOOUTDATA1_OUT4_SHIFT);

        // effects on/off
        GPIO_read ( GPIOHandle, CSL_GPIO_PIN12, &overdrive_status );    // overdrive
        GPIO_read ( GPIOHandle, CSL_GPIO_PIN14, &reverb_status );       // reverb

        if(overdrive_status){
            overdrive(&x[(pos[0][0])]);             // overdrive
        }
        if(reverb_status){
            reverb();                               // reverb
        }


        //GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_LOW); // 106 clock-cycles?
        // CSL_GPIO_REGS->IOOUTDATA1 = (CSL_GPIO_IOOUTDATA1_OUT4_CLEAR << CSL_GPIO_IOOUTDATA1_OUT4_SHIFT);
        aic3204_codec_write_mod( x[(pos[0][0])] );  // write analog value
        updatePos();                                // update array positions

        GPIO_clearInt(GPIOHandle, CSL_GPIO_PIN15);  // clear interrupt flag
    }
 /*
    while(1){ // This loop works

        // GPIO_write(GPIOHandle, CSL_GPIO_PIN12, GPIO_DRIVE_LOW);

        aic3204_codec_read_mod( &x[(pos[0][0])] );  // read analog value

        overdrive(&x[(pos[0][0])]);             // overdrive
        reverb();                               // reverb

        // GPIO_write(GPIOHandle, CSL_GPIO_PIN12, GPIO_DRIVE_HIGH);

        aic3204_codec_write_mod( x[(pos[0][0])] );  // write analog value
        updatePos();                                // update array positions
        GPIO_clearInt(GPIOHandle, CSL_GPIO_PIN15);
    }
    // test loop
        //ezdsp5535_GPIO_setDirection(GPIO31, GPIO_IN);
        while(1){
            // ezdsp5535_GPIO_setOutput(GPIO13, 1); // test high pin 4
            // reverb();
            // ezdsp5535_GPIO_setOutput(GPIO13, 0); // test low pin 4
            // ezdsp5535_waitusec( 200 );
            // printf("%i \n", x[(pos[0][0])]);
            // updatePos2();

            // updateVariables();
            // l = ezdsp5535_GPIO_getInput(GPIO31);
            //GPIO_open and GPIO_config or(GPIO_configBit) should be called before calling this API
            // GPIO_read(GPIOHandle, CSL_GPIO_PIN15, &l);
            // printf("%i \n", l);
            // l++;


            Uint16 test;
            SAR_readData(&SarObj1, &test);              // Q0.10
            printf("ADC value: %u \n ", test);
            ezdsp5535_waitusec( 200 );
            //GPIO_clearInt(GPIOHandle, CSL_GPIO_PIN15);
        }
*/
}

/////////////////////// Init function //////////////////////////////////////////
void pedal_init()
{
    // eZdsp - dev board
    ezdsp5535_init();

    // AIC3204 - audio codec
    aic3204_hardware_init();
    AIC3204_init_44100Hz();

    // GPAIN
    enableGPAINs();

    // GPIOs
    enableGPIOs();
    enableInterrupt();
    GPIO_clearInt(GPIOHandle, CSL_GPIO_PIN15);
}

///////////////////////////////////////// Overdrive ////////////////////////////
Int16 overdrive(Int16* signal){
    Int32 temp_OD = ( ( (Int32) gain ) * (*signal) ) >> 13; // (Q3.13 * Q1.15) >> 13 = Q3.15
    if (labs(temp_OD) > 32765){ // ensure Q1.15 with hardclip
        if( *signal > 0 ){ // positive
            *signal = 32767;
        }
        if( *signal < 0 ){ // negative
            *signal = - 32767;
        }
    }if(labs(temp_OD) < 32765){
        *signal = temp_OD;
    }

    /* linear, g(x) */
    if( abs(*signal) < lambda ){
        *signal = ( (Int32) linear_gain * (*signal) ) >> 14; // (Q2.14 * Q1.15) >> 14 = Q3.15 (Q1.15)
        return 0;
    }
    /* soft-clip, h(x) */
    if( abs( *signal ) >= lambda && abs( *signal ) < gamma ){
        xx  = (Int32) abs(*signal) * abs(*signal);  // Q1.15 * Q1.15 = Q2.30
        axx = (Int32) a * (xx >> 16);               // Q7.9  * Q2.14 = Q9.23
        bx  = ( (Int32) b * abs(*signal) ) >> 1 ;   // Q8.24 >> 1    = Q9.23
        c   = (Int32) c16 << 14;                    // Q23.9 << 14   = Q9.23

        if( *signal > 0 ){ // positive
            *signal =   (Int16)( ( -axx + bx -c) >> 8);    // (Q9.23 + Q9.23 + Q9.23) >> 8 = Q9.23
        }
        if( *signal < 0 ){ // negative
            *signal = ~((Int16)( (-axx + bx -c) >> 8));   // Q1.15
        }
        return 1;
    }
    /* hard-clip, i(x) */
    if( abs( *signal ) > gamma ){
        if( *signal > 0 ){ // positive
            *signal = 32767;
        }
        if( *signal < 0 ){ // negative
            *signal = - 32767;
        }
        return 2;
    }
    return 3;
}


/////////////////////////////// Reverb ///////////////////////////////
void reverb(){
// parallel combs
    C1[(pos[1][0])] = (Int16)( ( ( (Int32) a1 * C1[(pos[1][1])] ) >> 15 ) + x[(pos[0][1])] ); // ( Q1.15 * Q1.15 ) >> 15 + Q1.15 = Q2.15
    C2[(pos[2][0])] = (Int16)( ( ( (Int32) a2 * C2[(pos[2][1])] ) >> 15 ) + x[(pos[0][1])] );
    C3[(pos[3][0])] = (Int16)( ( ( (Int32) a3 * C3[(pos[3][1])] ) >> 15 ) + x[(pos[0][1])] );
    C4[(pos[4][0])] = (Int16)( ( ( (Int32) a4 * C4[(pos[4][1])] ) >> 15 ) + x[(pos[0][1])] );

// output of comb-section
    C_sum[(pos[5][0])] = (Int32) C1[(pos[1][1])] + C2[(pos[2][1])] + C3[(pos[3][1])] + C4[(pos[4][1])];
        // ensure C_sum in Q1.15
            if (labs( C_sum[(pos[5][0])] ) > 32767){
                if (C_sum[(pos[5][0])] > 0){ // positive
                    C_sum[(pos[5][0])] = 32767;
                }else{                       // negative
                    C_sum[(pos[5][0])] = -32767;
                }
            }

// series allpass
    Int32 temp_1, temp_2, temp_3;   // store temporary values

    // A1
        // Inner recursive, v1[n] = g * v1[n-m_A1] + C_sum[n-m_A1]
            temp_1 = ( (Int32) v1[(pos[5][1])] * g) >> 15;          //  (Q1.15 * Q1.15) >> 15   -> Q1.15
            temp_2 = C_sum[(pos[5][1])];                            //   Q1.15
            v1[(pos[5][0])] = temp_1 + temp_2;                      //   Q1.15 + Q1.15          -> Q1.15

        // Outer,           A1[n] = (1-g^2)*v1[n] - g* C_sum[n]
            temp_1 = (Int32) v1[(pos[5][0])] * g_mod;               //   Q1.15 * Q1.15          -> Q2.30
            temp_2 = C_sum[(pos[5][0])] * g;                        //   Q1.15 * Q1.15
            A1[(pos[6][0])] = ( (temp_1 - temp_2 ) >> 15);          //  (Q2.30 - Q2.30) >> 15   -> Q3.15

    // A2
        // Inner recursive, v2[n] = g * v2[n-m_A2] + A1[n-m_A2]
            temp_1 = (Int32) v2[(pos[6][1])] * g;                   //   Q4.12 * Q1.15          -> Q5.27
            temp_2 = A1[(pos[6][1])] << 12;                         //   Q3.15 << 12            -> Q3.27¨
            temp_3 = ( temp_1 + temp_2 ) >> 15;
            v2[(pos[6][0])] = ( temp_1 + temp_2 ) >> 15;            //  (Q5.27 + Q3.27) >> 15   -> Q4.12

        // Outer,           A2[n] = (1-g^2)*v2[n] - g* A1[n]
            temp_1 = (Int32) v2[(pos[6][0])] * g_mod;               //   Q4.12 * Q1.15          -> Q5.27
            temp_2 = ( A1[(pos[6][0])] >> 3)  * g;                  //  (Q3.15 >> 2) * Q1.15    -> Q5.27
            temp_3 = (temp_1 - temp_2) >> 12;                       //  (Q5.27 - Q5.27) >> 12   -> Q5.15
            A2     = ( (temp_3 >> 4) * alpha ) >> 11;               // ((Q5.15 >> 4) * Q1.15 ) >> 11    -> Q6.15

// Output
    // -60 dB threshold
            /*
        if( abs(A2) < 32){
            A2 = 0;
        }
        */

    // y[n] = x[n] + alpha * A2[n]
        temp_3 = ( (Int32) x[(pos[0][0])] ) + A2;       // ((Q1.15 << 13) + Q4.28) >> 15    -> Q4.15

    // overflow handler (ensure temp_3 in Q1.15)
        if ( labs(temp_3) > 32768){
            if (temp_3 > 0){ // positive
                temp_3 = 32767;
            }else{           // negative
                temp_3 = -32767;
            }
        }

    // update current signal-value
       x[(pos[0][0])] = temp_3;    // Q1.15
}

void updatePos(){
    pos[0][0]++;
    pos[1][0]++;
    pos[2][0]++;
    pos[3][0]++;
    pos[4][0]++;
    pos[5][0]++;
    pos[6][0]++;
    pos[0][1]++;
    pos[1][1]++;
    pos[2][1]++;
    pos[3][1]++;
    pos[4][1]++;
    pos[5][1]++;
    pos[6][1]++;
    if (pos[0][0] > 8820){
        pos[0][0] = 0;
    }
    if (pos[0][1] > 8820){
        pos[0][1] = 0;
    }
    if (pos[1][0] > 1323){
        pos[1][0] = 0;
    }
    if (pos[1][1] > 1323){
        pos[1][1] = 0;
    }
    if (pos[2][0] > 1456){
        pos[2][0] = 0;
    }
    if (pos[2][1] > 1456){
        pos[2][1] = 0;
    }
    if (pos[3][0] > 1808){
        pos[3][0] = 0;
    }
    if (pos[3][1] > 1808){
        pos[3][1] = 0;
    }
    if (pos[4][0] > 1984){
        pos[4][0] = 0;
    }
    if (pos[4][1] > 1984){
        pos[4][1] = 0;
    }
    if (pos[5][0] > 221){
        pos[5][0] = 0;
    }
    if (pos[5][1] > 221){
        pos[5][1] = 0;
    }
    if (pos[6][0] > 75){
        pos[6][0] = 0;
    }
    if (pos[6][1] > 75){
        pos[6][1] = 0;
    }
}

void clearArrays(){
    memset(  x, 0, sizeof( x ) );
    memset( C1, 0, sizeof( C1 ) );
    memset( C2, 0, sizeof( C2 ) );
    memset( C3, 0, sizeof( C3 ) );
    memset( C4, 0, sizeof( C4 ) );
    memset( C_sum, 0, sizeof( C_sum ) );
    memset( v1, 0, sizeof(v1) );
    memset( A1, 0, sizeof(A1) );
    memset( v2, 0, sizeof(v2) );
}


//////////////////////// GPAIN and GPIO /////////////////////////////

// GPAIN setup (channel number in DSP Datasheet p. 34)

CSL_SarHandleObj  SarObj = { // GPAIN 3, green wire (isolated)
    CSL_SAR_REGS,       // address of SAR registers
    CSL_SAR_CHAN_3,     // channel
    CSL_SOK             // status
};

CSL_SarChSetup  SarParam = {
    0,                      // sysclk division
    CSL_SAR_POLLING,        // interrupt mode
    CSL_SAR_NO_DISCHARGE,   // multichannel type
    CSL_SAR_REF_VIN         // ref voltage
};

void enableGPAINs(){
    // reading analog values from MUX08
    SAR_chanOpen(&SarObj, CSL_SAR_CHAN_5);                  // open channel
    SAR_GPODirSet(&SarObj, CSL_SAR_GPO_3, CSL_SAR_GPO_IN);  // set direction
    SAR_chanSetup(&SarObj,&SarParam);                       // setup (polling, no discharge, and Vin as reference)
    SAR_startConversion(&SarObj);                           // start ADC
}

// GPIO setup
CSL_GpioObj GPIOobj = {
    CSL_GPIO_REGS,
    CSL_GPIO_NUM_PIN
};

// interrupt GPIO
    CSL_GpioPinConfig config_Int = {
        /** Pin number for GPIO     */
        CSL_GPIO_PIN15,

        /** Direction for GPIO Pin  */
        CSL_GPIO_DIR_INPUT,

        /** GPIO pin edge detection */
        CSL_GPIO_TRIG_RISING_EDGE
    };


// switching in MUX08
    // A0 (furthest from keypin)
        CSL_GpioPinConfig config_A0 = {
            CSL_GPIO_PIN4,
            CSL_GPIO_DIR_OUTPUT,
            CSL_GPIO_TRIG_CLEAR_EDGE
        };
    // A1
        CSL_GpioPinConfig config_A1 = {
            CSL_GPIO_PIN5,
            CSL_GPIO_DIR_OUTPUT,
            CSL_GPIO_TRIG_CLEAR_EDGE
        };
    // A2
        CSL_GpioPinConfig config_A2 = {
            CSL_GPIO_PIN10,
            CSL_GPIO_DIR_OUTPUT,
            CSL_GPIO_TRIG_CLEAR_EDGE
        };

// enable/disable effects
    // Overdrive
        CSL_GpioPinConfig config_OD = {
            CSL_GPIO_PIN12,
            CSL_GPIO_DIR_INPUT,
            CSL_GPIO_TRIG_CLEAR_EDGE
        };
    // Reverb
        CSL_GpioPinConfig config_RE = {
            CSL_GPIO_PIN14,
            CSL_GPIO_DIR_INPUT,
            CSL_GPIO_TRIG_CLEAR_EDGE
        };

void enableGPIOs(){
    GPIOHandle = GPIO_open(&GPIOobj, &status);

    // Open interrupt GPIO
    GPIO_configBit(GPIOHandle, &config_Int);
    GPIO_enableInt (GPIOHandle, CSL_GPIO_PIN15);    // only interrupt on PIN15

    // Open MUX switching GPIOs
    GPIO_configBit(GPIOHandle, &config_A0);
    GPIO_configBit(GPIOHandle, &config_A1);
    GPIO_configBit(GPIOHandle, &config_A2);

    // Open enable/disable effects GPIOs
    GPIO_configBit(GPIOHandle, &config_OD);
    GPIO_configBit(GPIOHandle, &config_RE);

    // GP[0:5] and GP[6:11] set as GPIOs
    CSL_SYSCTRL_REGS->EBSR = CSL_SYSCTRL_REGS->EBSR | (CSL_SYS_EBSR_SP1MODE_MODE2 << CSL_SYS_EBSR_SP1MODE_SHIFT);
    CSL_SYSCTRL_REGS->EBSR = CSL_SYSCTRL_REGS->EBSR | (CSL_SYS_EBSR_SP0MODE_MODE2 << CSL_SYS_EBSR_SP0MODE_SHIFT);

}

///////////////////////////  Interrupt //////////////////////////////////////

// Interrupt setup
void enableInterrupt(){
    IRQ_globalDisable();                    // No interrupts while setup is running
    IRQ_clearAll();                         // clear all flags
    IRQ_setVecs(((Uint32) &VECSTART));      // Set IVPH/IVPD to start of interrupt vector table
    IRQ_plug(GPIO_EVENT, &updateVariables); // setup updateVariables(); to run, when GPIO_EVENT
    IRQ_enable(GPIO_EVENT);                 // enable the setup
    IRQ_globalEnable();                     // enable interrupts
}

Uint16 temp_delay = 0;

// ISR
interrupt void updateVariables(void){

// Overdrive
    // Overdrive gain
        // S1, LLL
            GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_LOW);     // A0
            GPIO_write(GPIOHandle, CSL_GPIO_PIN5, GPIO_DRIVE_LOW);     // A1
            GPIO_write(GPIOHandle, CSL_GPIO_PIN10, GPIO_DRIVE_LOW);    // A2
            ezdsp5535_waitusec( 10 );   // wait more than switch time (max 2.1 us)
        // read
            SAR_readData(&SarObj, &updateVariables_temp);              // Q0.10
        // calculate
            gain = ( ( (Int32) 2 * updateVariables_temp  + 1) << 3 );  // (Q3.0 * Q0.10) << 3 = Q3.13

    // Overdrive threshold
        // S2, LLH
            GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_HIGH);     // A0
            GPIO_write(GPIOHandle, CSL_GPIO_PIN5, GPIO_DRIVE_LOW);     // A1
            GPIO_write(GPIOHandle, CSL_GPIO_PIN10, GPIO_DRIVE_LOW);   // A2
            ezdsp5535_waitusec( 10 );   // wait more than switch time (max 2.1 us)
        // read
            SAR_readData(&SarObj, &updateVariables_temp);              // Q0.10
        // calculate
            float lambda_float = 0.33 + 0.27/1023 * updateVariables_temp;
            lambda = (Int16) (lambda_float * 32768);                   // threshold, Q1.15

            double temp_abc = 1/( pow( (5*lambda_float-4), 2) );
            a   = (Int16) ( temp_abc * 4 * 512);                                                                // a, Q7.9
            b   = (Int16) ( (temp_abc * ( pow(lambda_float, 2) * 36.25 - 50 * lambda_float +23.2 ) ) * 512);    // b, Q7.9
            c16 = (Int16) ( (temp_abc * 4 * pow(lambda_float, 2) ) * 512 );                                     // c, Q7.9

// Reverb
    // Pre-delay
        // S5, HLL
            GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_LOW);    // A0
            GPIO_write(GPIOHandle, CSL_GPIO_PIN5, GPIO_DRIVE_LOW);     // A1
            GPIO_write(GPIOHandle, CSL_GPIO_PIN10, GPIO_DRIVE_HIGH);    // A2
            ezdsp5535_waitusec( 10 );   // wait more than switch time (max 2.1 us)
        // read
            SAR_readData(&SarObj, &updateVariables_temp);              // Q0.10
        // calculate
            pre_delay = ( ( (Int32) updateVariables_temp ) * 8820 ) >> 10; // Q16.0
            temp_delay = pos[0][0] + 8820 - pre_delay;
            if(temp_delay > 8820){
                temp_delay = temp_delay - 8820;
            }
            // pos[0][1] = (Uint16) temp_delay;         // OBS [0; 8811]

    // decay time
        // S6, HLH
            GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_HIGH);    // A0
            GPIO_write(GPIOHandle, CSL_GPIO_PIN5, GPIO_DRIVE_LOW);     // A1
            GPIO_write(GPIOHandle, CSL_GPIO_PIN10, GPIO_DRIVE_HIGH);   // A2
            ezdsp5535_waitusec( 10 );   // wait more than switch time (max 2.1 us)
        // read
            SAR_readData(&SarObj, &updateVariables_temp);              // Q0.10
        // calculate
            decay_time = (double) updateVariables_temp / 1023 * 2;     // decay_time float
            double decay_time_exp = 0.09/decay_time;                   // exponent for a1 calculation
            double a_temp = pow(0.1, decay_time_exp);                  // temporary a1
            a1 = (Int16) ( a_temp * 32768 );              // Q1.15
            a2 = (Int16) ( pow(a_temp, 1.1) * 32768 );    // Q1.15
            a3 = (Int16) ( pow(a_temp, 1.37) * 32768 );   // Q1.15
            a4 = (Int16) ( pow(a_temp, 1.5) * 32768 );    // Q1.15

    // Reverb dry/wet mix
        // S7, HHL
            GPIO_write(GPIOHandle, CSL_GPIO_PIN4, GPIO_DRIVE_LOW);    // A0
            GPIO_write(GPIOHandle, CSL_GPIO_PIN5, GPIO_DRIVE_HIGH);    // A1
            GPIO_write(GPIOHandle, CSL_GPIO_PIN10, GPIO_DRIVE_HIGH);    // A2
            ezdsp5535_waitusec( 10 );   // wait more than switch time (max 2.1 us)
        // read
            SAR_readData(&SarObj, &updateVariables_temp);   // Q0.10
        // calculate
            alpha = updateVariables_temp << 5;              // Q0.10 << Q1.15 (always positive, sign-bit = 0)

            pedal_init();
}

//////////////////////// read/write functions /////////////////////////
void aic3204_codec_read_mod(Int16* input)
{
    volatile Int16 dummy;
    Int16 temp1, temp2;

    while(!(I2S2_IR & RcvR) ){}         // while no interrupt flag register
    temp1 = I2S2_W0_MSW_R;
    dummy = I2S2_W0_LSW_R;             // Read Least Significant Word (ignore)
    temp2 = I2S2_W1_MSW_R;
    dummy = I2S2_W1_LSW_R;             // Read Least Significant Word of second channel (ignore)
    //*input = (temp1+temp2) / 2;
    *input = temp1;

}

void aic3204_codec_write_mod(Int16 output)
{
    while( !(I2S2_IR & XmitR) ) {}  // while no interrupt flag register
    I2S2_W0_MSW_W = output;         // Left output most significant word
    I2S2_W0_LSW_W = 0;
    I2S2_W1_MSW_W = output;         // Right output most significant word
    I2S2_W1_LSW_W = 0;
}

/////////////////////// codec setup ///////////////////////////////////

void AIC3204_init_44100Hz()
{
    /* Configure Parallel Port */
    SYS_EXBUSSEL = 0x1000;  // Configure Parallel Port mode = 1 for I2S2

    /* Configure AIC3204 */
    AIC3204_rset( 0, 0 );      // Select page 0
    AIC3204_rset( 1, 1 );      // Reset codec
    AIC3204_rset( 0, 1 );      // Point to page 1
    AIC3204_rset( 1, 8 );      // Disable crude AVDD generation from DVDD
    AIC3204_rset( 2, 1 );      // Enable Analog Blocks, use LDO power
    AIC3204_rset( 0, 0 );

    /* PLL and Clocks config and Power Up  */
    AIC3204_rset( 27, 0x1d );               // BCLK and WCLK is set as o/p to AIC3204(Master)
    AIC3204_rset( 28, 0x00 );               // Data ofset = 0
    AIC3204_rset( 4, 3 );                   // PLL setting: PLLCLK <- MCLK, CODEC_CLKIN <-PLL CLK
    AIC3204_rset( 6, 7 );                   // PLL setting: J=7
    AIC3204_rset( 7, 0x02 );                // PLL setting: HI_BYTE(D)
    AIC3204_rset( 8, 0x30 );                // PLL setting: LO_BYTE(D)
    AIC3204_rset( 30, 0x88 );               // For 32 bit clocks per frame in Master mode ONLY
                                            // BCLK=DAC_CLK/N =(12288000/8) = 1.536MHz = 32*fs
    AIC3204_rset( 5, 1 << 7 | 1 << 4 | 1);  //PLL setting: Power up PLL, P=1 and R=1
    AIC3204_rset( 13, 0 );                  // Hi_Byte(DOSR) for DOSR = 128 decimal or 0x0080 DAC oversamppling
    AIC3204_rset( 14, 128 );                // Lo_Byte(DOSR) for DOSR = 128 decimal or 0x0080
    AIC3204_rset( 20, 128 );                // AOSR for AOSR = 128 decimal or 0x0080 for decimation filters 1 to 6
    AIC3204_rset( 11, 1 << 7 | 5 );         // Power up NDAC and set NDAC value to 7
    AIC3204_rset( 12, 1 << 7 | 3 );         // Power up MDAC and set MDAC value to 2
    AIC3204_rset( 18, 1 << 7 | 5 );         // Power up NADC and set NADC value to 7
    AIC3204_rset( 19, 1 << 7 | 3 );         // Power up MADC and set MADC value to 2

    /* DAC ROUTING and Power Up */
    AIC3204_rset( 0, 1 );      // Select page 1
    AIC3204_rset( 0x0c, 8 );   // LDAC AFIR routed to HPL
    AIC3204_rset( 0x0d, 8 );   // RDAC AFIR routed to HPR
    AIC3204_rset( 0, 0 );      // Select page 0
    AIC3204_rset( 64, 2 );     // Left vol=right vol
    AIC3204_rset( 65, 0 );     // Left DAC gain to 0dB VOL; Right tracks Left
    AIC3204_rset( 63, 0xd4 );  // Power up left,right data paths and set channel
    AIC3204_rset( 0, 1 );      // Select page 1
    AIC3204_rset( 0x10, 0 );   // Unmute HPL , 10dB gain
    AIC3204_rset( 0x11, 0 );   // Unmute HPR , 10dB gain
    AIC3204_rset( 9, 0x30 );   // Power up HPL,HPR
    AIC3204_rset( 0, 0 );      // Select page 0
    ezdsp5535_wait( 100 );     // wait

    /* ADC ROUTING and Power Up */
    AIC3204_rset( 0, 1 );           // Select page 1
    AIC3204_rset( 51, 0x48);        // power up MICBIAS with AVDD (0x40)or LDOIN (0x48)   //MM - added micbias
    AIC3204_rset( 0x34, 0x10 );     // STEREO 1 Jack
                                    // IN2_L to LADC_P through 0 kohm
    AIC3204_rset( 0x37, 0x10 );     // IN2_R to RADC_P through 0 kohmm
    AIC3204_rset( 0x36, 1 );        // CM_1 (common mode) to LADC_M through 0 kohm
    AIC3204_rset( 0x39, 0x40 );     // CM_1 (common mode) to RADC_M through 0 kohm
    AIC3204_rset( 0x3b, 1 << 7 );   // MIC_PGA_L unmute - 0dB gain
    AIC3204_rset( 0x3c, 1 << 7 );   // MIC_PGA_R unmute - 0dB gain
    AIC3204_rset( 0, 0 );           // Select page 0
    AIC3204_rset( 0x51, 0xc0 );     // Powerup Left and Right ADC
    AIC3204_rset( 0x52, 0 );        // Unmute Left and Right ADC
    AIC3204_rset( 0, 0 );           // Select page 0
    ezdsp5535_wait( 100 );          // Wait

    /* I2S settings */
    I2S2_SRGR = 0x0;
    I2S2_CR = 0x8010;    // 16-bit word, slave, enable I2C
    I2S2_ICMR = 0x3f;    // Enable interrupts
}
