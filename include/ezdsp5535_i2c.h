//////////////////////////////////////////////////////////////////////////////
// * File name: ezdsp5535_i2c.h
// *                                                                          
// * Description:  I2C header file.
// *                                                                          
// * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/ 
// * Copyright (C) 2010 Spectrum Digital, Incorporated
// *                                                                          
// *                                                                          
// *  Redistribution and use in source and binary forms, with or without      
// *  modification, are permitted provided that the following conditions      
// *  are met:                                                                
// *                                                                          
// *    Redistributions of source code must retain the above copyright        
// *    notice, this list of conditions and the following disclaimer.         
// *                                                                          
// *    Redistributions in binary form must reproduce the above copyright     
// *    notice, this list of conditions and the following disclaimer in the   
// *    documentation and/or other materials provided with the                
// *    distribution.                                                         
// *                                                                          
// *    Neither the name of Texas Instruments Incorporated nor the names of   
// *    its contributors may be used to endorse or promote products derived   
// *    from this software without specific prior written permission.         
// *                                                                          
// *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS     
// *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT       
// *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR   
// *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT    
// *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,   
// *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT        
// *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,   
// *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY   
// *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT     
// *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE   
// *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.    
// *                                                                          
//////////////////////////////////////////////////////////////////////////////

#ifndef EZDSP5535_I2C_H_
#define EZDSP5535_I2C_H_

#include "ezdsp5535.h"


/* ------------------------------------------------------------------------ *
 *  Prototypes                                                              *
 * ------------------------------------------------------------------------ */
// Function prototypes
Int16 ezdsp5535_I2C_init ( );
Int16 ezdsp5535_I2C_close( );
Int16 ezdsp5535_I2C_read( Uint16 i2c_addr, Uint8* data, Uint16 len );
Int16 ezdsp5535_I2C_write( Uint16 i2c_addr, Uint8* data, Uint16 len );
Int16 ezdsp5535_I2C_write_SR( Uint16 i2c_addr, Uint8* data, Uint16 len );
Int16 ezdsp5535_I2C_reset( );

#define MDR_STT         0x2000 // Mode Register start condition bit
#define MDR_TRX         0x0200 // Mode Register transmitter mode bit
#define MDR_MST         0x0400 // Mode Register master mode bit (1=master mode, 0=slave mode)
#define MDR_IRS         0x0020 // Mode Register i2c reset bit
#define MDR_FREE        0x4000 // Mode Register emulation mode bit (when 1 the i2c continues at breakpoints)
#define STR_XRDY        0x0010 // Intterupt status register transmit data ready bit
#define STR_RRDY        0x0008 // Intterupt status register receive data ready bit
#define MDR_STP         0x0800 // Mode Register stop condition bit
#endif /*EZDSP5535_I2C_H_*/
