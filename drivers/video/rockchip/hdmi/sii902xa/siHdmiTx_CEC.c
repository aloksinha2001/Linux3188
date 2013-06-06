//***************************************************************************
//!file     si_apiCEC.c
//!brief    Silicon Image mid-level CEC handler
//
// No part of this work may be reproduced, modified, distributed,
// transmitted, transcribed, or translated into any language or computer
// format, in any form or by any means without written permission of
// Silicon Image, Inc., 1060 East Arques Avenue, Sunnyvale, California 94085
//
// Copyright 2009, Silicon Image, Inc.  All rights reserved.
//***************************************************************************/
#include <linux/i2c.h>

#include "siHdmiTx_902x_TPI.h"

#ifdef DEV_SUPPORT_CEC

//bool si_CecRxMsgHandlerARC ( SI_CpiData_t *pCpi );  // Warning: Starter Kit Specific
bool CpCecRxMsgHandler ( SI_CpiData_t *pCpi );      // Warning: Starter Kit Specific

//-------------------------------------------------------------------------------
// CPI Enums, typedefs, and manifest constants
//-------------------------------------------------------------------------------
//#define CEC_SLAVE_ADDR 0xC0
extern struct i2c_client *siiCEC;


#define SET_BITS    0xFF
#define CLEAR_BITS  0x00

#define BIT0                    0x01
#define BIT1                    0x02
#define BIT2                    0x04
#define BIT3                    0x08
#define BIT4                    0x10
#define BIT5                    0x20
#define BIT6                    0x40
#define BIT7                    0x80

#define CEC_NO_TEXT         0
#define CEC_NO_NAMES        1
#define CEC_ALL_TEXT        2
#define INCLUDE_CEC_NAMES   CEC_NO_TEXT

    /* New Source Task internal states    */

enum
{
    NST_START               = 1,
    NST_SENT_PS_REQUEST,
    NST_SENT_PWR_ON,
    NST_SENT_STREAM_REQUEST
};

    /* Task CPI states. */

enum
{
    CPI_IDLE            = 1,
    CPI_SENDING,
    CPI_WAIT_ACK,
    CPI_WAIT_RESPONSE,
    CPI_RESPONSE
};

typedef struct
{
    byte     task;       // Current CEC task
    byte     state;      // Internal task state
    byte     cpiState;   // State of CPI transactions
    byte     destLA;     // Logical address of target device
    byte     taskData1;  // BYTE Data unique to task.
    word taskData2;  // WORD Data unique to task.

} CEC_TASKSTATE;

//------------------------------------------------------------------------------
// Data
//------------------------------------------------------------------------------

CEC_DEVICE   g_childPortList [SII_NUMBER_OF_PORTS];

//-------------------------------------------------------------------------------
// Module data
//-------------------------------------------------------------------------------

byte     g_cecAddress            = CEC_LOGADDR_PLAYBACK1;       //2011.08.03 logical address CEC_LOGADDR_TV // default Initiator is TV
word g_cecPhysical           = 0x0000;               // For TV, the physical address is 0.0.0.0

static byte  g_currentTask       = SI_CECTASK_IDLE;

static bool l_cecEnabled        = FALSE;
static byte  l_powerStatus       = CEC_POWERSTATUS_ON;
static byte  l_portSelect        = 0x00;
static byte  l_sourcePowerStatus = CEC_POWERSTATUS_STANDBY;

    /* Active Source Addresses  */

static byte  l_activeSrcLogical  = CEC_LOGADDR_UNREGORBC;    // Logical address of our active source
static word l_activeSrcPhysical = 0x0000;

static byte  l_devTypes [16] =
{
    CEC_LOGADDR_TV,
    CEC_LOGADDR_RECDEV1,
    CEC_LOGADDR_RECDEV1,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_PLAYBACK1,
    CEC_LOGADDR_AUDSYS,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_PLAYBACK1,
    CEC_LOGADDR_RECDEV1,
    CEC_LOGADDR_TUNER1,
    CEC_LOGADDR_PLAYBACK1,
    0x02,
    0x02,
    CEC_LOGADDR_TV,
    CEC_LOGADDR_TV
};

    /* CEC task data    */

static byte l_newTask            = FALSE;
static CEC_TASKSTATE l_cecTaskState = { 0 };
static CEC_TASKSTATE l_cecTaskStateQueued =
{
    SI_CECTASK_ENUMERATE,           // Current CEC task
    0,                              // Internal task state
    CPI_IDLE,                       // cpi state
    0x00,                           // Logical address of target device
    0x00,                           // BYTE Data unique to task.
    0x0000                          // WORD Data unique to task.
};


/*
//#include "Defs.h"
//#include "i2c_master_sw.h"
//------------------------------------------------------------------------------
// Function Name: ReadByteTPI()
// Function Description: I2C read
//------------------------------------------------------------------------------
static byte I2CReadByte(struct i2c_client *client, byte RegOffset)
{
	byte Readnum;
	Readnum = i2c_smbus_read_byte_data(client,RegOffset);
	return Readnum;
}

//------------------------------------------------------------------------------
// Function Name: WriteByteTPI()
// Function Description: I2C write
//------------------------------------------------------------------------------
static void I2CWriteByte (struct i2c_client *client, byte RegOffset, byte Data)
{
    	i2c_smbus_write_byte_data(client,RegOffset, Data);
}
*/
extern byte I2CReadByte ( byte SlaveAddr, byte RegAddr );
extern void I2CWriteByte ( byte SlaveAddr, byte RegAddr, byte Data );
extern byte I2CReadBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data );
extern byte I2CWriteBlock( byte SlaveAddr, byte RegAddr, byte NBytes, byte * Data );
//------------------------------------------------------------------------------
// Function:    SiIRegioReadBlock
// Description: Read a block of registers starting with the specified register.
//              The register address parameter is translated into an I2C
//              slave address and offset.
//              The block of data bytes is read from the I2C slave address
//              and offset.
//------------------------------------------------------------------------------

void SiIRegioReadBlock ( word regAddr, byte* buffer, word length)
{
    I2CReadBlock(CEC_SLAVE_ADDR, (byte)regAddr, length,buffer );
//    HalI2cBus0ReadBlock( l_regioDecodePage[ regAddr >> 8], (byte)regAddr, buffer, length);
}
//------------------------------------------------------------------------------
// Function:    SiIRegioWriteBlock
// Description: Write a block of registers starting with the specified register.
//              The register address parameter is translated into an I2C slave
//              address and offset.
//              The block of data bytes is written to the I2C slave address
//              and offset.
//------------------------------------------------------------------------------

void SiIRegioWriteBlock ( word regAddr, byte* buffer, word length)
{
    I2CWriteBlock(CEC_SLAVE_ADDR, (byte)regAddr, length,buffer );
    //HalI2cBus0WriteBlock( l_regioDecodePage[ regAddr >> 8], (byte)regAddr, buffer, length);
}

//------------------------------------------------------------------------------
// Function:    SiIRegioRead
// Description: Read a one byte register.
//              The register address parameter is translated into an I2C slave
//              address and offset. The I2C slave address and offset are used
//              to perform an I2C read operation.
//------------------------------------------------------------------------------

byte SiIRegioRead ( word regAddr )
{
    return (I2CReadByte(CEC_SLAVE_ADDR, (byte)regAddr));
//	byte Readnum;
//	Readnum = i2c_smbus_read_byte_data(siiCEC,(byte)regAddr);
	//TPI_TRACE_PRINT(("regAddr=0x%04X,Readnum=0x%04X\n", (byte)regAddr,Readnum));
//	return Readnum;
}

//------------------------------------------------------------------------------
// Function:    SiIRegioWrite
// Description: Write a one byte register.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset
//              are used to perform an I2C write operation.
//------------------------------------------------------------------------------

void SiIRegioWrite ( word regAddr, byte value )
{
    I2CWriteByte(CEC_SLAVE_ADDR, (byte)regAddr, value);
	//i2c_smbus_write_byte_data(siiCEC, (byte)regAddr, value);
	//TPI_TRACE_PRINT(("regAddr=0x%04X,value=0x%04X\n", (byte)regAddr,value));
}


//------------------------------------------------------------------------------
// Function:    SiIRegioModify
// Description: Modify a one byte register under mask.
//              The register address parameter is translated into an I2C
//              slave address and offset. The I2C slave address and offset are
//              used to perform I2C read and write operations.
//
//              All bits specified in the mask are set in the register
//              according to the value specified.
//              A mask of 0x00 does not change any bits.
//              A mask of 0xFF is the same a writing a byte - all bits
//              are set to the value given.
//              When only some bits in the mask are set, only those bits are
//              changed to the values given.
//------------------------------------------------------------------------------

void SiIRegioModify ( word regAddr, byte mask, byte value)
{
    byte abyte;

    abyte = I2CReadByte( CEC_SLAVE_ADDR, (byte)regAddr );
//    abyte = i2c_smbus_read_byte_data(siiCEC,(byte)regAddr);
    abyte &= (~mask);                                       //first clear all bits in mask
    abyte |= (mask & value);                                //then set bits from value
    I2CWriteByte( CEC_SLAVE_ADDR, (byte)regAddr, abyte);
//	i2c_smbus_write_byte_data(siiCEC, (byte)regAddr, value);
}

//------------------------------------------------------------------------------
// Function:    SI_CpiSetLogicalAddr
// Description: Configure the CPI subsystem to respond to a specific CEC
//              logical address.
//------------------------------------------------------------------------------

bool SI_CpiSetLogicalAddr ( byte logicalAddress )
{
    byte capture_address[2];
    byte capture_addr_sel = 0x01;

    capture_address[0] = 0;
    capture_address[1] = 0;
    if( logicalAddress < 8 )
    {
        capture_addr_sel = capture_addr_sel << logicalAddress;
        capture_address[0] = capture_addr_sel;
    }
    else
    {
        capture_addr_sel   = capture_addr_sel << ( logicalAddress - 8 );
        capture_address[1] = capture_addr_sel;
    }

        // Set Capture Address

    SiIRegioWriteBlock(REG_CEC_CAPTURE_ID0, capture_address, 2 );
    SiIRegioWrite( REG_CEC_TX_INIT, logicalAddress );
	SiIRegioRead( REG_CEC_TX_INIT);
    TPI_TRACE_PRINT(("********** logicalAddress: 0x%0x\n", (int)SiIRegioRead( REG_CEC_TX_INIT))); 
    TPI_TRACE_PRINT(("********** CEC Enable: 0x%0x\n", (int)SiIRegioRead(0x88E))); 

    return( TRUE );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiSendPing
// Description: Initiate sending a ping, and used for checking available
//                       CEC devices
//------------------------------------------------------------------------------

void SI_CpiSendPing ( byte bCECLogAddr )
{
    SiIRegioWrite( REG_CEC_TX_DEST, BIT_SEND_POLL | bCECLogAddr );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiWrite
// Description: Send CEC command via CPI register set
//------------------------------------------------------------------------------

bool SI_CpiWrite( SI_CpiData_t *pCpi )
{
    byte cec_int_status_reg[2];

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
    CpCecPrintCommand( pCpi, TRUE );
#endif
    SiIRegioModify( REG_CEC_DEBUG_3, BIT_FLUSH_TX_FIFO, BIT_FLUSH_TX_FIFO );  // Clear Tx Buffer

        /* Clear Tx-related buffers; write 1 to bits to be cleared. */

    cec_int_status_reg[0] = 0x64 ; // Clear Tx Transmit Buffer Full Bit, Tx msg Sent Event Bit, and Tx FIFO Empty Event Bit
    cec_int_status_reg[1] = 0x02 ; // Clear Tx Frame Retranmit Count Exceeded Bit.
    SiIRegioWriteBlock( REG_CEC_INT_STATUS_0, cec_int_status_reg, 2 );

        /* Send the command */

    SiIRegioWrite( REG_CEC_TX_DEST, pCpi->srcDestAddr & 0x0F );           // Destination
    SiIRegioWrite( REG_CEC_TX_COMMAND, pCpi->opcode );
	
    SiIRegioWriteBlock( REG_CEC_TX_OPERAND_0, pCpi->args, pCpi->argCount );
    SiIRegioWrite( REG_CEC_TRANSMIT_DATA, BIT_TRANSMIT_CMD | pCpi->argCount );

	//2011.08.03 for CEC debug print Begin
	{
		byte i;
		CPI_DEBUG_PRINT( ( "\TX CEC: SrcDesAdd:%02x, opcode:%02X,", (int)(pCpi->srcDestAddr), (int)(pCpi->opcode)));
		for(i=0;i<(pCpi->argCount);i++)
		{
			CPI_DEBUG_PRINT(("%02x",(int)(pCpi->args[i])));
		}
		CPI_DEBUG_PRINT(("\n"));
	}
	//2011.08.03 End
    return( TRUE );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiRead
// Description: Reads a CEC message from the CPI read FIFO, if present.
//------------------------------------------------------------------------------

bool SI_CpiRead( SI_CpiData_t *pCpi )
{
    bool    error = FALSE;
    byte argCount;

    argCount = SiIRegioRead( REG_CEC_RX_COUNT );

    if ( argCount & BIT_MSG_ERROR )
    {
        error = TRUE;
    }
    else
    {
        pCpi->argCount = argCount & 0x0F;
        pCpi->srcDestAddr = SiIRegioRead( REG_CEC_RX_CMD_HEADER );
        pCpi->opcode = SiIRegioRead( REG_CEC_RX_OPCODE );
        if ( pCpi->argCount )
        {
            SiIRegioReadBlock( REG_CEC_RX_OPERAND_0, pCpi->args, pCpi->argCount );
        }
		
	//2011.08.03 for CEC debug print Begin
	{
		byte i;
    		CPI_DEBUG_PRINT( ( "\RX CEC: SrcDesAdd:%02x, opcode:%02X,", (int)(pCpi->srcDestAddr), (int)(pCpi->opcode)));
		for(i=0;i<(pCpi->argCount);i++)
		{
			CPI_DEBUG_PRINT(("%02x",(int)(pCpi->args[i])));
		}
		CPI_DEBUG_PRINT(("\n"));
	}
	//2011.08.03 End
    }

        // Clear CLR_RX_FIFO_CUR;
        // Clear current frame from Rx FIFO

    SiIRegioModify( REG_CEC_RX_CONTROL, BIT_CLR_RX_FIFO_CUR, BIT_CLR_RX_FIFO_CUR );

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
    if ( !error )
    {
        CpCecPrintCommand( pCpi, FALSE );
    }
#endif
    return( error );
}


//------------------------------------------------------------------------------
// Function:    SI_CpiStatus
// Description: Check CPI registers for a CEC event
//------------------------------------------------------------------------------

bool SI_CpiStatus( SI_CpiStatus_t *pStatus )
{
    byte cecStatus[2];

    pStatus->txState    = 0;
    pStatus->cecError   = 0;
    pStatus->rxState    = 0;

    SiIRegioReadBlock( REG_CEC_INT_STATUS_0, cecStatus, 2);

    if ( (cecStatus[0] & 0x7F) || cecStatus[1] )
    {
        CPI_DEBUG_PRINT(("\nCEC Status: %02X %02X\n", (int) cecStatus[0], (int) cecStatus[1]));
		

            // Clear interrupts

        if ( cecStatus[1] & BIT_FRAME_RETRANSM_OV )
        {
            CPI_DEBUG_PRINT(("\n!TX retry count exceeded! [%02X][%02X]\n",(int) cecStatus[0], (int) cecStatus[1]));

                /* Flush TX, otherwise after clearing the BIT_FRAME_RETRANSM_OV */
                /* interrupt, the TX command will be re-sent.                   */

            SiIRegioModify( REG_CEC_DEBUG_3,BIT_FLUSH_TX_FIFO, BIT_FLUSH_TX_FIFO );
        }

            // Clear set bits that are set

        SiIRegioWriteBlock( REG_CEC_INT_STATUS_0, cecStatus, 2 );

            // RX Processing

        if ( cecStatus[0] & BIT_RX_MSG_RECEIVED )
        {
            pStatus->rxState = 1;   // Flag caller that CEC frames are present in RX FIFO
        }

            // RX Errors processing

        if ( cecStatus[1] & BIT_SHORT_PULSE_DET )
        {
            pStatus->cecError |= SI_CEC_SHORTPULSE;
        }

        if ( cecStatus[1] & BIT_START_IRREGULAR )
        {
            pStatus->cecError |= SI_CEC_BADSTART;
        }

        if ( cecStatus[1] & BIT_RX_FIFO_OVERRUN )
        {
            pStatus->cecError |= SI_CEC_RXOVERFLOW;
        }

            // TX Processing

        if ( cecStatus[0] & BIT_TX_FIFO_EMPTY )
        {
            pStatus->txState = SI_TX_WAITCMD;
        }
        if ( cecStatus[0] & BIT_TX_MESSAGE_SENT )
        {
            pStatus->txState = SI_TX_SENDACKED;
        }
        if ( cecStatus[1] & BIT_FRAME_RETRANSM_OV )
        {
            pStatus->txState = SI_TX_SENDFAILED;
        }
    }

    return( TRUE );
}

//------------------------------------------------------------------------------
// Function:    SI_CpiGetLogicalAddr
// Description: Get Logical Address
//------------------------------------------------------------------------------

byte SI_CpiGetLogicalAddr( void )
{
    return SiIRegioRead( REG_CEC_TX_INIT);
}

//------------------------------------------------------------------------------
// Function:    SI_CpiInit
// Description: Initialize the CPI subsystem for communicating via CEC
//------------------------------------------------------------------------------

bool SI_CpiInit( void )
{
	TPI_TRACE_PRINT(("\n>>SI_CpiInit()\n"));

#ifdef DEV_SUPPORT_CEC_FEATURE_ABORT
    // Turn on CEC auto response to <Abort> command.
    SiIRegioWrite( CEC_OP_ABORT_31, BIT7 );
	SiIRegioRead( CEC_OP_ABORT_31);
#else
    // Turn off CEC auto response to <Abort> command.
    SiIRegioWrite( CEC_OP_ABORT_31, CLEAR_BITS );
#endif

#ifdef DEV_SUPPORT_CEC_CONFIG_CPI_0
	SiIRegioModify (REG_CEC_CONFIG_CPI, 0x10, 0x00);
#endif

    // initialized he CEC CEC_LOGADDR_TV logical address

    //2011.08.03 logical address- if ( !SI_CpiSetLogicalAddr( CEC_LOGADDR_PLAYBACK1 ))
    //{
	SI_CecEnumerate(SI_CECTASK_StartActive,0);//2011.06.17 logical address //2011.06.14 Send Image view on, Active Source
    //    CPI_DEBUG_PRINT( ("\n Cannot init CPI/CEC"));
    //    return( FALSE );
    //}

    //Enable CEC INT
    SiIRegioWrite( REG_CEC_INT_ENABLE_0,  0x27);//0x26
    SiIRegioWrite( REG_CEC_INT_ENABLE_1, 0x2F );//2//0x2E

    return( TRUE );
}



//------------------------------------------------------------------------------
// Function:    CecViewOn
// Description: Take the HDMI switch and/or sink device out of standby and
//              enable it to display video.
//------------------------------------------------------------------------------
//2011.08.03 delete
//static void CecViewOn ( SI_CpiData_t *pCpi )
//{
//    pCpi = pCpi;
//
//    SI_CecSetPowerState( CEC_POWERSTATUS_ON );
//}

//------------------------------------------------------------------------------
// Function:    CpCecRxMsgHandler
// Description: Parse received messages and execute response as necessary
//              Only handle the messages needed at the top level to interact
//              with the Port Switch hardware.  The SI_API message handler
//              handles all messages not handled here.
//
// Warning:     This function is referenced by the Silicon Image CEC API library
//              and must be present for it to link properly.  If not used,
//              it should return 0 (FALSE);
//
//              Returns TRUE if message was processed by this handler
//------------------------------------------------------------------------------

//bool CpCecRxMsgHandler ( SI_CpiData_t *pCpi )
//{
//    bool            processedMsg, isDirectAddressed;
//
//    isDirectAddressed = !((pCpi->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );
//
//    processedMsg = TRUE;
//    if ( isDirectAddressed )
//    {
//        switch ( pCpi->opcode )
//        {
//            case CECOP_IMAGE_VIEW_ON:       // In our case, respond the same to both these messages
//            case CECOP_TEXT_VIEW_ON:
//                CecViewOn( pCpi );
//                break;
//
//            case CECOP_INACTIVE_SOURCE:
//                break;
//            default:
//                processedMsg = FALSE;
//                break;
//        }
//    }
//
//    /* Respond to broadcast messages.   */
//
//    else
//    {
//        switch ( pCpi->opcode )
//        {
//            case CECOP_ACTIVE_SOURCE:
//                break;
//            default:
//                processedMsg = FALSE;
//            break;
//    }
//    }
//
//    return( processedMsg );
//}

#if 0
(INCLUDE_CEC_NAMES > CEC_NO_TEXT)

//------------------------------------------------------------------------------
// Function:    CpCecPrintCommand
// Description: Translate CEC transaction into human-readable form.
//------------------------------------------------------------------------------

bool CpCecPrintCommand ( SI_CpiData_t *pMsg, bool_t isTX )
    {
    bool        success = false;
#if (INCLUDE_CEC_NAMES == CEC_ALL_TEXT)
    char        logStr [120];
#else
    char        logStr [60];
#endif
    word    writeCnt;
    int         i;


    writeCnt = sprintf(
        logStr, "CEC:: [%02X][%c%02X](%d): ",
        (word)pMsg->opcode, isTX ? 'W' : 'R', (word)pMsg->srcDestAddr, (word)pMsg->argCount
        );
    for ( i = 0; i < pMsg->argCount; i++ )
        writeCnt = sprintf( logStr, "%s%02X ", logStr, (word)pMsg->args[ i] );

#if (INCLUDE_CEC_NAMES > CEC_NO_NAMES)

    /* Add human readable command name  */

    sprintf(
        logStr, "%s%*s%s",
        logStr,
        52 - writeCnt, "; ",
        CpCecTranslateOpcodeName( pMsg )
        );
#endif
    sprintf( logStr, "%s\n", logStr );

    CPI_DEBUG_PRINT( ( logStr ));

    return( success );
    }
#endif // (INCLUDE_CEC_NAMES > CEC_NO_TEXT)

//------------------------------------------------------------------------------
// Function:
// Description:
//------------------------------------------------------------------------------

static void PrintLogAddr ( byte bLogAddr )
{

#if (INCLUDE_CEC_NAMES > CEC_NO_NAMES)
    CPI_DEBUG_PRINT((  " [%X] %s", (int)bLogAddr, CpCecTranslateLA( bLogAddr ) ));
#else
    bLogAddr = 0;   // Passify the compiler
#endif
}

//------------------------------------------------------------------------------
// Function:    UpdateChildPortList
// Description: Store the PA and LA of the subsystem if it is at the next
//              next level down from us (direct child).
//              Returns the HDMI port index that is hosting this physical address
//------------------------------------------------------------------------------

static void UpdateChildPortList ( byte newLA, word newPA )
{
    byte     index;
    word    mask, pa;

    /* Determine Physical Address position (A.B.C.D) of our */
    /* device and generate a mask.                          */
    /* Field D cannot host any child devices.               */

    mask = 0x00F0;
    for ( index = 1; index < 4; index++ )
    {
        if (( g_cecPhysical & mask ) != 0)
            break;
        mask <<= 4;
    }

    /* Re-initialize the child port list with possible physical     */
    /* addresses of our immediate child devices.                    */
    /* If the new physical address is one of our direct children,   */
    /* update the LA for that position in the child port list.      */

    mask    = 0x0001 << ((index - 1) * 4);
    pa      = mask;
    for ( index = 0; index < SII_NUMBER_OF_PORTS; index++)
    {
        g_childPortList[ index].cecPA = pa;
        if ( pa == newPA )
        {
            g_childPortList[ index].cecLA = newLA;
            g_childPortList[ index].deviceType = l_devTypes[newLA];
            CPI_DEBUG_PRINT(("\n***** newPA: %04X, newLA: %02D", (int)newPA, (int)newLA));
        }
        pa += mask;
    }
}

//------------------------------------------------------------------------------
// Function:    CecGetPortIndex
// Description: Return the index within the child port list of the passed port.
//
//              Returns 0xFF if port is not in the child list
//------------------------------------------------------------------------------

//static byte CecGetPortIndex ( word port )
//{
//    word    temp;
//    byte     i;
//
//    port++; // need 1-based value.
//
//    // Look for non-zero nibble postion in source port physical address
//
//    temp = 0x000F;
//   // Look for non-zero nibble postion in source port physical address
//    for ( i = 0; i < 4; i++)
//    {
//        if ((g_cecPhysical & temp) != 0)
//                break;
//        temp <<= 4;
//    }
//
//    //If i is equal to zero means no more child (or end of the node)
//    if (i != 0)
//    {
//        port <<= (4 * (i-1));
//        temp = g_cecPhysical | port;        // PA of the current EDID
//    }
//    else
//    {
//        temp = g_cecPhysical;
//    }
//
//    for ( i = 0; i < SII_NUMBER_OF_PORTS; i++)
//    {
//        if ( g_childPortList[ i ].cecPA == temp )
//        {
//            return( g_childPortList[ i ].cecLA );
//        }
//    }
//    return( 0xFF );
//}

//------------------------------------------------------------------------------
// Function:    CecSendUserControlPressed
// Description: Local function for sending a "remote control key pressed"
//              command to the specified source.
//------------------------------------------------------------------------------

static void CecSendUserControlPressed ( byte keyCode, byte destLA  )
{
    SI_CpiData_t cecFrame;

    cecFrame.opcode        = CECOP_USER_CONTROL_PRESSED;
    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, destLA );
    cecFrame.args[0]       = keyCode;
    cecFrame.argCount      = 1;
    SI_CpiWrite( &cecFrame );
}

//------------------------------------------------------------------------------
// Function:    CecSendFeatureAbort
// Description: Send a feature abort as a response to this message unless
//              it was broadcast (illegal).
//------------------------------------------------------------------------------

static void CecSendFeatureAbort ( SI_CpiData_t *pCpi, byte reason )
{
    SI_CpiData_t cecFrame;

    if (( pCpi->srcDestAddr & 0x0F) != CEC_LOGADDR_UNREGORBC )
    {
        cecFrame.opcode        = CECOP_FEATURE_ABORT;
        cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, (pCpi->srcDestAddr & 0xF0) >> 4 );
        cecFrame.args[0]       = pCpi->opcode;
        cecFrame.args[1]       = reason;
        cecFrame.argCount      = 2;
        SI_CpiWrite( &cecFrame );
    }
}

//------------------------------------------------------------------------------
// Function:    CecHandleFeatureAbort
// Description: Received a failure response to a previous message.  Print a
//              message and notify the rest of the system
//------------------------------------------------------------------------------

#if (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
static char *ml_abortReason [] =        // (0x00) <Feature Abort> Opcode    (RX)
    {
    "Unrecognized OpCode",              // 0x00
    "Not in correct mode to respond",   // 0x01
    "Cannot provide source",            // 0x02
    "Invalid Operand",                  // 0x03
    "Refused"                           // 0x04
    };
#endif

static void CecHandleFeatureAbort( SI_CpiData_t *pCpi )
{
    SI_CpiData_t cecFrame;

    cecFrame.opcode = pCpi->args[0];
    cecFrame.argCount = 0;
#if (INCLUDE_CEC_NAMES > CEC_NO_NAMES)
    CPI_DEBUG_PRINT(
         (
        "\nMessage %s(%02X) was %s by %s (%d)\n",
        CpCecTranslateOpcodeName( &cecFrame ), (int)pCpi->args[0],
        ml_abortReason[ (pCpi->args[1] <= CECAR_REFUSED) ? pCpi->args[1] : 0],
        CpCecTranslateLA( pCpi->srcDestAddr >> 4 ), (int)(pCpi->srcDestAddr >> 4)
        ));
#elif (INCLUDE_CEC_NAMES > CEC_NO_TEXT)
    CPI_DEBUG_PRINT(
        (
        "\nMessage %02X was %s by logical address %d\n",
        (int)pCpi->args[0],
        ml_abortReason[ (pCpi->args[1] <= CECAR_REFUSED) ? pCpi->args[1] : 0],
        (int)(pCpi->srcDestAddr >> 4)
        ));
#endif
}

//------------------------------------------------------------------------------
// Function:    CecHandleActiveSource
// Description: Process the CEC Active Source command by switching to the
//              broadcast port.
//------------------------------------------------------------------------------

static void CecHandleActiveSource ( SI_CpiData_t *pCpi )
{
    byte     i;
    word    newPA;

    /* Extract the logical and physical addresses of the new active source. */

    l_activeSrcLogical  = (pCpi->srcDestAddr >> 4) & 0x0F;
    l_activeSrcPhysical = ((word)pCpi->args[0] << 8 ) | pCpi->args[1];

    UpdateChildPortList( l_activeSrcLogical, l_activeSrcPhysical );

    /* Determine the index of the HDMI port that    */
    /* is handling this physical address.           */

    newPA = l_activeSrcPhysical;
    for ( i = 0; i < 4; i++ )
    {
        if (( newPA & 0x000F ) != 0)
            break;
        newPA >>= 4;
    }

    /* Port address (1-based) is left in the lowest nybble. */
    /* Convert to 0-based and use it.                       */
    /* Signal main process of new port.  The main process   */
    /* will perform the physical port switch.               */

    l_portSelect = (( newPA & 0x000F ) - 1 );
    CPI_DEBUG_PRINT( ( "\nACTIVE_SOURCE: %02X (%04X) (port %02X)\n", (int)l_activeSrcLogical, l_activeSrcPhysical, (int)l_portSelect ));
}

//------------------------------------------------------------------------------
// Function:    CecHandleInactiveSource
// Description: Process the CEC Inactive Source command
//------------------------------------------------------------------------------

static void CecHandleInactiveSource ( SI_CpiData_t *pCpi )
{
    byte la;

    la = (pCpi->srcDestAddr >> 4) & 0x0F;
    if ( la == l_activeSrcLogical )    // The active source has deserted us!
    {
        l_activeSrcLogical  = CEC_LOGADDR_TV;
        l_activeSrcPhysical = 0x0000;
    }

    l_portSelect = 0xFF;    // Tell main process to choose another port.
}

//------------------------------------------------------------------------------
// Function:    CecHandleReportPhysicalAddress
// Description: Store the PA and LA of the subsystem.
//              This routine is called when a physical address was broadcast.
//              usually this routine is used for a system which configure as TV or Repeater.
//------------------------------------------------------------------------------

static void CecHandleReportPhysicalAddress ( SI_CpiData_t *pCpi )
{
    UpdateChildPortList(
        (pCpi->srcDestAddr >> 4) & 0xF,         // broadcast logical address
        (pCpi->args[0] << 8) | pCpi->args[1]    // broadcast physical address
        );
}
//2011.08.03 logical address Begin
void nextTaskSelect(byte next, byte nextnext)
{
	switch(next)
		{
		case SI_CECTASK_StartActive:
			SI_StartActiveSource(nextnext,0);
			break;
		default:
			break;
		}
}
//------------------------------------------------------------------------------
// Function:    CecTaskEnumerate
// Description: Ping every logical address on the CEC bus to see if anything
//              is attached.  Code can be added to store information about an
//              attached device, but we currently do nothing with it.
//
//              As a side effect, we determines our Initiator address as
//              the first available address of 0x00, 0x0E or 0x0F.
//
// l_cecTaskState.taskData1 == Not Used.
// l_cecTaskState.taskData2 == Not used
//------------------------------------------------------------------------------
byte abEnumTblDVD[4] = { 3, CEC_LOGADDR_PLAYBACK1, CEC_LOGADDR_PLAYBACK2, CEC_LOGADDR_PLAYBACK3 };
byte CECLogicTable;
byte CecTaskEnumerate ( SI_CpiStatus_t *pCecStatus )
{

	byte newTask = l_cecTaskState.task;
	SI_CpiData_t    cecFrame;

	//1. Ping Process, Note: In Ping Process, cpiState is always in CPI_WAIT_ACK status
	if(l_cecTaskState.state == 0  )
	{
		if ( pCecStatus->txState == SI_TX_SENDFAILED )
		{
			CPI_DEBUG_PRINT( ( "\nEnum NoAck %x\n" ,(int)(l_cecTaskState.destLA )));
			g_cecAddress = l_cecTaskState.destLA;
			CPI_DEBUG_PRINT( ( "ENUM DONE: Initiator address is %x\n",(int)g_cecAddress ));
				
			//Go to Report physical Address
			l_cecTaskState.state =1; 
			l_cecTaskState.cpiState = CPI_IDLE;
		
		}
		else if ( pCecStatus->txState == SI_TX_SENDACKED )
		{
			CPI_DEBUG_PRINT( ( "\n---------------> Enum Ack %x\n",(int)( l_cecTaskState.destLA ) ));
			CECLogicTable++;

			if(CECLogicTable<abEnumTblDVD[0])
			{
				l_cecTaskState.destLA = abEnumTblDVD[CECLogicTable+1];
				SI_CpiSetLogicalAddr( l_cecTaskState.destLA );
				SI_CpiSendPing( l_cecTaskState.destLA );
				CPI_DEBUG_PRINT(  ( "...Ping...%x\n" ,(int)( l_cecTaskState.destLA )));
			}
			else
			{
				g_cecAddress = CEC_LOGADDR_UNREGORBC;
				SI_CpiSetLogicalAddr( g_cecAddress );
				CPI_DEBUG_PRINT( ( "ENUM DONE: Initiator address is UNREGORBC" ));

				//Go to Report physical Address 
				//l_cecTaskState.state =1; 
				//l_cecTaskState.cpiState = CPI_IDLE;
				newTask = SI_CECTASK_IDLE;	//would do nothing
			}
		}
	}

	

	//2.Broadcast my physical address
	if(l_cecTaskState.state == 1  )
	{
		if(l_cecTaskState.cpiState == CPI_IDLE)
		{

			cecFrame.opcode        = CECOP_REPORT_PHYSICAL_ADDRESS;
			cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
			cecFrame.args[0]       = (SI_CecGetDevicePA()&0xFF00)>>8;             // [Physical Address]
			cecFrame.args[1]       = (SI_CecGetDevicePA()&0x00FF);             // [Physical Address]
			cecFrame.args[2]       = g_cecAddress;
			cecFrame.argCount      = 3;
			SI_CpiWrite( &cecFrame );
			l_cecTaskState.cpiState = CPI_WAIT_ACK;
		}
		else
		{
			if ( pCecStatus->txState == SI_TX_SENDACKED )
			{
				l_cecTaskState.state =2; //Go to broadcast Device Vendor ID 
				l_cecTaskState.cpiState = CPI_IDLE;
			}
			else	//SI_TX_SENDFAILED
			{
				CPI_DEBUG_PRINT( ( "!!!CEC Error, Not sent physical address\n" ));
				newTask = SI_CECTASK_IDLE;
			}
		}

	}
	
	//3.Broadcast My Vendor ID
	if(l_cecTaskState.state ==2)
	{
		if(l_cecTaskState.cpiState == CPI_IDLE)
		{
			cecFrame.opcode        = CECOP_DEVICE_VENDOR_ID;
			cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
			cecFrame.args[0]       = 0x00;
			cecFrame.args[1]       = 0x4D;
			cecFrame.args[2]       = 0x29;
			cecFrame.argCount      = 3;
			SI_CpiWrite( &cecFrame );
			l_cecTaskState.cpiState = CPI_WAIT_ACK;
		}
		else
		{
			if ( pCecStatus->txState == SI_TX_SENDACKED )
			{
				l_cecTaskState.state =3; //Go to Get Menu Language 
				l_cecTaskState.cpiState = CPI_IDLE;
			}
			else	//SI_TX_SENDFAILED
			{
				CPI_DEBUG_PRINT( ( "!!!CEC Error, Not sent Vendor ID\n" ));
				newTask = SI_CECTASK_IDLE;
			}
		}
	}	//	l_cecTaskState.cpiState = CPI_IDLE;

	//4.Get Menu Language from TV
	if(l_cecTaskState.state ==3)
	{
		if(l_cecTaskState.cpiState == CPI_IDLE)
		{
			si_CecSendMessage(CECOP_GET_MENU_LANGUAGE,CEC_LOGADDR_TV);

			l_cecTaskState.cpiState = CPI_WAIT_ACK;
		}
		else
		{
			if ( pCecStatus->txState == SI_TX_SENDACKED )
			{
				//l_cecTaskState.state =3; //Go to Get Menu Language 
			}
			else	//SI_TX_SENDFAILED
			{
				CPI_DEBUG_PRINT( ( "!!!CEC Error, Not sent Vendor ID\n" ));
			}
			newTask = SI_CECTASK_IDLE;
		}

	}	//	l_cecTaskState.cpiState = CPI_IDLE;

	if((newTask== SI_CECTASK_IDLE)&&(l_cecTaskState.taskData1!=0))
	{
		nextTaskSelect(l_cecTaskState.taskData1,l_cecTaskState.taskData2);
		newTask = l_cecTaskState.task;
	}

	return( newTask );
	

#if 0	
    byte newTask = l_cecTaskState.task;

    g_cecAddress = CEC_LOGADDR_UNREGORBC;

    if ( l_cecTaskState.cpiState == CPI_IDLE )
    {
        if ( l_cecTaskState.destLA < CEC_LOGADDR_UNREGORBC )   // Don't ping broadcast address
        {
            SI_CpiSendPing( l_cecTaskState.destLA );
            CPI_DEBUG_PRINT(  ( "...Ping...\n" ));
            PrintLogAddr( l_cecTaskState.destLA );
            l_cecTaskState.cpiState = CPI_WAIT_ACK;
        }
        else    // We're done
        {
            SI_CpiSetLogicalAddr( g_cecAddress );
            CPI_DEBUG_PRINT( ( "ENUM DONE: Initiator address is " ));
            PrintLogAddr( g_cecAddress );
            CPI_DEBUG_PRINT( ( "\n" ));

                /* Go to idle task, we're done. */

            l_cecTaskState.cpiState = CPI_IDLE;
            newTask = SI_CECTASK_IDLE;
        }
    }
    else if ( l_cecTaskState.cpiState == CPI_WAIT_ACK )
    {
        if ( pCecStatus->txState == SI_TX_SENDFAILED )
        {
            CPI_DEBUG_PRINT( ( "\nEnum NoAck" ));
            PrintLogAddr( l_cecTaskState.destLA );

            /* If a TV address, grab it for our use.    */

            if (( g_cecAddress == CEC_LOGADDR_UNREGORBC ) &&
                (( l_cecTaskState.destLA == CEC_LOGADDR_TV ) ||
                ( l_cecTaskState.destLA == CEC_LOGADDR_FREEUSE )))
            {
                g_cecAddress = l_cecTaskState.destLA;
            }

            /* Restore Tx State to IDLE to try next address.    */

            l_cecTaskState.cpiState = CPI_IDLE;
            l_cecTaskState.destLA++;
        }
        else if ( pCecStatus->txState == SI_TX_SENDACKED )
        {
            CPI_DEBUG_PRINT( ( "\n-----------------------------------------------> Enum Ack\n" ));
            PrintLogAddr( l_cecTaskState.destLA );

            /* TODO: Add code here to store info about this device if needed.   */

            /* Restore Tx State to IDLE to try next address.    */

            l_cecTaskState.cpiState = CPI_IDLE;
            l_cecTaskState.destLA++;
        }
    }

    return( newTask );
#endif
}

//------------------------------------------------------------------------------
// l_cecTaskState.state =0===>Send Image View On
// l_cecTaskState.state =1===>Send Active Source
//------------------------------------------------------------------------------
byte  CECTaskStartActive( SI_CpiStatus_t *pCecStatus )
{
	byte newTask = l_cecTaskState.task;

	//Image View On
	CPI_DEBUG_PRINT( ( "!!!CECTaskStartActive %x %2x\n" ,(int)(l_cecTaskState.state),(int)(l_cecTaskState.cpiState)));
	if(l_cecTaskState.state ==0)
	{
		if(l_cecTaskState.cpiState == CPI_IDLE)
		{
			SI_CecSendImageViewOn();
			//SI_CecSendActiveSource();

			l_cecTaskState.cpiState = CPI_WAIT_ACK;
		}
		else//CPI_WAIT_ACK
		{
			if ( pCecStatus->txState == SI_TX_SENDACKED )
			{
				CPI_DEBUG_PRINT( ( "\nOK On Image View On\n" ));

			}
			else	//SI_TX_SENDFAILED
			{
				CPI_DEBUG_PRINT( ( "!!!CEC Error, Image View On\n" ));
			}
			l_cecTaskState.state=1;//Start to send Active Source
			l_cecTaskState.cpiState = CPI_IDLE;
			newTask = SI_CECTASK_IDLE;
		}

	}	
/*
	//Give Power Status
	if(l_cecTaskState.state ==1)
	{
		if(l_cecTaskState.cpiState == CPI_IDLE)
		{
			si_CecSendMessage(CECOP_GIVE_DEVICE_POWER_STATUS,CEC_LOGADDR_TV);
			CPI_DEBUG_PRINT( ( "Send Give Power Status\n" ));

			l_cecTaskState.cpiState = CPI_WAIT_ACK;
		}
		else
		{
			if ( pCecStatus->txState == SI_TX_SENDACKED )
			{
				CPI_DEBUG_PRINT( ( "\nOK On Give Power Status\n" ));

			}
			else	//SI_TX_SENDFAILED
			{
				CPI_DEBUG_PRINT( ( "!!!CEC Error, Give Power Status\n" ));
			}
			l_cecTaskState.state=2;
			l_cecTaskState.cpiState = CPI_IDLE;
		}

	}	
*/
	//Active source
	if(l_cecTaskState.state ==1)
	{
		if(l_cecTaskState.cpiState == CPI_IDLE)
		{
			SI_CecSendActiveSource();
			//SI_CecSendImageViewOn();

			l_cecTaskState.cpiState = CPI_WAIT_ACK;
		}
		else
		{
			if ( pCecStatus->txState == SI_TX_SENDACKED )
			{
				CPI_DEBUG_PRINT( ( "\nOK On Active Source\n" ));

			}
			else	//SI_TX_SENDFAILED
			{
				CPI_DEBUG_PRINT( ( "!!!CEC Error, Active Source\n" ));
			}
			l_cecTaskState.state=2;
			newTask = SI_CECTASK_IDLE;
		}

	}	
	
	return (newTask);

}
//2011.08.03 Enumerate judgement End

//------------------------------------------------------------------------------
// Function:    CecTaskStartNewSource
// Description: Request power status from a device.  If in standby, tell it
//              to turn on and wait until it does, then start the device
//              streaming to us.
//
// l_cecTaskState.taskData1 == Number of POWER_ON messages sent.
// l_cecTaskState.taskData2 == Physical address of target device.
//------------------------------------------------------------------------------
//2011.08.03 delete
//static byte CecTaskStartNewSource ( SI_CpiStatus_t *pCecStatus )
//{
//    SI_CpiData_t    cecFrame;
//    byte         newTask = l_cecTaskState.task;
//
//    if ( l_cecTaskState.cpiState == CPI_IDLE )
//    {
//        if ( l_cecTaskState.state == NST_START )
//        {
//                /* First time through here, TIMER_2 will not have   */
//                /* been started yet, and therefore should return    */
//                /* expired (0 == 0).                                */
//
//            //if ( HalTimerExpired( TIMER_2 ))
//            {
//                CPI_DEBUG_PRINT( ( "\nNST_START\n" ));
//                si_CecSendMessage( CECOP_GIVE_DEVICE_POWER_STATUS, l_cecTaskState.destLA );
//
//                l_cecTaskState.cpiState = CPI_WAIT_ACK;
//                l_cecTaskState.state    = NST_SENT_PS_REQUEST;
//            }
//        }
//    }
//        /* Wait for acknowledgement of message sent.    */
//
//    else if ( l_cecTaskState.cpiState == CPI_WAIT_ACK )
//    {
//        if ( pCecStatus->txState == SI_TX_SENDFAILED )
//        {
//            CPI_DEBUG_PRINT( ( "New Source Task NoAck\n" ));
//
//                /* Abort task */
//
//            l_cecTaskState.cpiState = CPI_IDLE;
//            newTask                 = SI_CECTASK_IDLE;
//        }
//        else if ( pCecStatus->txState == SI_TX_SENDACKED )
//        {
//            CPI_DEBUG_PRINT( ( "New Source Task ACK\n" ));
//
//            if ( l_cecTaskState.state == NST_SENT_PS_REQUEST )
//            {
//                l_cecTaskState.cpiState  = CPI_WAIT_RESPONSE;
//            }
//            else if ( l_cecTaskState.state == NST_SENT_PWR_ON )
//            {
//                    /* This will force us to start over with status request.    */
//
//                l_cecTaskState.cpiState = CPI_IDLE;
//                l_cecTaskState.state    = NST_START;
//                l_cecTaskState.taskData1++;
//            }
//            else if ( l_cecTaskState.state == NST_SENT_STREAM_REQUEST )
//            {
//                    /* Done.    */
//
//                l_cecTaskState.cpiState = CPI_IDLE;
//                newTask                 = SI_CECTASK_IDLE;
//            }
//        }
//    }
//
//        /* Looking for a response to our message. During this time, we do nothing.  */
//        /* The various message handlers will set the cpi state to CPI_RESPONSE when */
//        /* a device has responded with an appropriate message.                      */
//
//    else if ( l_cecTaskState.cpiState == CPI_WAIT_RESPONSE )
//    {
//        //Lee: Need to have a timeout here.
//    }
//        /* We have an answer. Go to next step.  */
//
//    else if ( l_cecTaskState.cpiState == CPI_RESPONSE )
//    {
//        if ( l_cecTaskState.state == NST_SENT_PS_REQUEST )    // Acking GIVE_POWER_STATUS
//        {
//                /* l_sourcePowerStatus is updated by the REPORT_POWER_STATUS message.   */
//
//            switch ( l_sourcePowerStatus )
//            {
//                case CEC_POWERSTATUS_ON:
//                    {
//                            /* Device power is on, tell device to send us some video.   */
//
//                        cecFrame.opcode         = CECOP_SET_STREAM_PATH;
//                        cecFrame.srcDestAddr    = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
//                        cecFrame.args[0]        = (byte)(l_cecTaskState.taskData2 >> 8);
//                        cecFrame.args[1]        = (byte)l_cecTaskState.taskData2;
//                        cecFrame.argCount       = 2;
//                        SI_CpiWrite( &cecFrame );
//                        l_cecTaskState.cpiState = CPI_WAIT_ACK;
//                        l_cecTaskState.state    = NST_SENT_STREAM_REQUEST;
//                        break;
//                    }
//                case CEC_POWERSTATUS_STANDBY:
//                case CEC_POWERSTATUS_ON_TO_STANDBY:
//                    {
//                            /* Device is in standby, tell it to turn on via remote control. */
//
//                        if ( l_cecTaskState.taskData1 == 0 )
//                        {
//                            CecSendUserControlPressed( CEC_RC_POWER, l_cecTaskState.destLA );
//                            l_cecTaskState.cpiState = CPI_WAIT_ACK;
//                            l_cecTaskState.state    = NST_SENT_PWR_ON;
//                            //HalTimerSet( TIMER_2, 500 );
//                        }
//                        else    // Abort task if already tried to turn it on device without success
//                        {
//                            l_cecTaskState.cpiState = CPI_IDLE;
//                            newTask                 = SI_CECTASK_IDLE;
//                        }
//                        break;
//                    }
//                case CEC_POWERSTATUS_STANDBY_TO_ON:
//                    {
//                        l_cecTaskState.cpiState = CPI_IDLE;
//                        l_cecTaskState.state    = NST_START;
//                        //HalTimerSet( TIMER_2, 500 );   // Wait .5 seconds between requests for status
//                        break;
//                    }
//            }
//        }
//    }
//
//    return( newTask );
//}

//------------------------------------------------------------------------------
// Function:    ValidateCecMessage
// Description: Validate parameter count of passed CEC message
//              Returns FALSE if not enough operands for the message
//              Returns TRUE if the correct amount or more of operands (if more
//              the message processor willl just ignore them).
//------------------------------------------------------------------------------

static bool ValidateCecMessage ( SI_CpiData_t *pCpi )
{
    byte parameterCount = 0;
    bool    countOK = TRUE;

    /* Determine required parameter count   */

    switch ( pCpi->opcode )
    {
        case CECOP_IMAGE_VIEW_ON:
        case CECOP_TEXT_VIEW_ON:
        case CECOP_STANDBY:
        case CECOP_GIVE_PHYSICAL_ADDRESS:
        case CECOP_GIVE_DEVICE_POWER_STATUS:
        case CECOP_GET_MENU_LANGUAGE:
        case CECOP_GET_CEC_VERSION:
            parameterCount = 0;
            break;
        case CECOP_REPORT_POWER_STATUS:         // power status
        case CECOP_CEC_VERSION:                 // cec version
            parameterCount = 1;
            break;
        case CECOP_INACTIVE_SOURCE:             // physical address
        case CECOP_FEATURE_ABORT:               // feature opcode / abort reason
        case CECOP_ACTIVE_SOURCE:               // physical address
            parameterCount = 2;
            break;
        case CECOP_REPORT_PHYSICAL_ADDRESS:     // physical address / device type
        case CECOP_DEVICE_VENDOR_ID:            // vendor id
            parameterCount = 3;
            break;
        case CECOP_SET_OSD_NAME:                // osd name (1-14 bytes)
        case CECOP_SET_OSD_STRING:              // 1 + x   display control / osd string (1-13 bytes)
            parameterCount = 1;                 // must have a minimum of 1 operands
            break;
        case CECOP_ABORT:
            break;

        case CECOP_ARC_INITIATE:
            break;
        case CECOP_ARC_REPORT_INITIATED:
            break;
        case CECOP_ARC_REPORT_TERMINATED:
            break;

        case CECOP_ARC_REQUEST_INITIATION:
            break;
        case CECOP_ARC_REQUEST_TERMINATION:
            break;
        case CECOP_ARC_TERMINATE:
            break;
        default:
            break;
    }

    /* Test for correct parameter count.    */

    if ( pCpi->argCount < parameterCount )
    {
        countOK = FALSE;
    }

    return( countOK );
}

//------------------------------------------------------------------------------
// Function:    si_CecRxMsgHandlerFirst
// Description: This is the first message handler called in the chain.
//              It parses messages we don't want to pass on to outside handlers
//              and those that we need to look at, but not exclusively
//------------------------------------------------------------------------------

//static bool si_CecRxMsgHandlerFirst ( SI_CpiData_t *pCpi )
//{
//    bool            msgHandled, isDirectAddressed;
//
//    /* Check messages we handle for correct number of operands and abort if incorrect.  */
//
//    msgHandled = TRUE;
//    if ( ValidateCecMessage( pCpi ))    // If invalid message, ignore it, but treat it as handled
//    {
//        isDirectAddressed = !((pCpi->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );
//        if ( isDirectAddressed )
//        {
//            switch ( pCpi->opcode )
//            {
//                case CECOP_INACTIVE_SOURCE:
//                    CecHandleInactiveSource( pCpi );
//                    msgHandled = FALSE;             // Let user also handle it if they want.
//                    break;
//
//                default:
//                    msgHandled = FALSE;
//                    break;
//            }
//        }
//        else
//        {
//            switch ( pCpi->opcode )
//            {
//                case CECOP_ACTIVE_SOURCE:
//                    CecHandleActiveSource( pCpi );
//                    msgHandled = FALSE;             // Let user also handle it if they want.
//                    break;
//                default:
//                    msgHandled = FALSE;
//                    break;
//            }
//        }
//    }
//
//    return( msgHandled );
//}

//------------------------------------------------------------------------------
// Function:    si_CecRxMsgHandlerLast
// Description: This is the last message handler called in the chain, and
//              parses any messages left untouched by the previous handlers.
//------------------------------------------------------------------------------

static bool si_CecRxMsgHandlerLast ( SI_CpiData_t *pCpi )
{
    bool            isDirectAddressed;
    SI_CpiData_t    cecFrame;

    isDirectAddressed = !((pCpi->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );

    if ( ValidateCecMessage( pCpi ))            // If invalid message, ignore it, but treat it as handled
    {
        if ( isDirectAddressed )
        {
            switch ( pCpi->opcode )
            {
                case CECOP_FEATURE_ABORT:
                    CecHandleFeatureAbort( pCpi );
                    break;

                case CECOP_IMAGE_VIEW_ON:       // In our case, respond the same to both these messages
                case CECOP_TEXT_VIEW_ON:
                    break;

                case CECOP_STANDBY:             // Direct and Broadcast

                        /* Setting this here will let the main task know    */
                        /* (via SI_CecGetPowerState) and at the same time   */
                        /* prevent us from broadcasting a STANDBY message   */
                        /* of our own when the main task responds by        */
                        /* calling SI_CecSetPowerState( STANDBY );          */

                    l_powerStatus = CEC_POWERSTATUS_STANDBY;
                    break;

                case CECOP_INACTIVE_SOURCE:
                    CecHandleInactiveSource( pCpi );
                    break;

                case CECOP_GIVE_PHYSICAL_ADDRESS:

                    /* TV responds by broadcasting its Physical Address: 0.0.0.0   */

                    cecFrame.opcode        = CECOP_REPORT_PHYSICAL_ADDRESS;
                    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
                    cecFrame.args[0]       = (SI_CecGetDevicePA()&0xFF00)>>8;             // [Physical Address]
                    cecFrame.args[1]       = (SI_CecGetDevicePA()&0x00FF);             // [Physical Address]
                    cecFrame.args[2]       = CEC_LOGADDR_PLAYBACK1;//2011.08.03 CEC_LOGADDR_TV;   // [Device Type] = 0 = TV
                    cecFrame.argCount      = 3;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_GIVE_DEVICE_POWER_STATUS:

                    /* TV responds with power status.   */

                    cecFrame.opcode        = CECOP_REPORT_POWER_STATUS;
                    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, (pCpi->srcDestAddr & 0xF0) >> 4 );
                    cecFrame.args[0]       = l_powerStatus;
                    cecFrame.argCount      = 1;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_GET_MENU_LANGUAGE:

                    /* TV Responds with a Set Menu language command.    */

                    cecFrame.opcode         = CECOP_SET_MENU_LANGUAGE;
                    cecFrame.srcDestAddr    = CEC_LOGADDR_UNREGORBC;
                    cecFrame.args[0]        = 'e';     // [language code see iso/fdis 639-2]
                    cecFrame.args[1]        = 'n';     // [language code see iso/fdis 639-2]
                    cecFrame.args[2]        = 'g';     // [language code see iso/fdis 639-2]
                    cecFrame.argCount       = 3;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_GET_CEC_VERSION:

                    /* TV responds to this request with it's CEC version support.   */

                    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, (pCpi->srcDestAddr & 0xF0) >> 4 );
                    cecFrame.opcode        = CECOP_CEC_VERSION;
                    cecFrame.args[0]       = 0x04;       // Report CEC1.3a
                    cecFrame.argCount      = 1;
                    SI_CpiWrite( &cecFrame );
                    break;

                case CECOP_REPORT_POWER_STATUS:         // Someone sent us their power state.

                    l_sourcePowerStatus = pCpi->args[0];

                        /* Let NEW SOURCE task know about it.   */

                    if ( l_cecTaskState.task == SI_CECTASK_NEWSOURCE )
                    {
                        l_cecTaskState.cpiState = CPI_RESPONSE;
                    }
                    break;

                /* Do not reply to directly addressed 'Broadcast' msgs.  */

                case CECOP_ACTIVE_SOURCE:
                case CECOP_REPORT_PHYSICAL_ADDRESS:     // A physical address was broadcast -- ignore it.
                case CECOP_REQUEST_ACTIVE_SOURCE:       // We are not a source, so ignore this one.
                case CECOP_ROUTING_CHANGE:              // We are not a downstream switch, so ignore this one.
                case CECOP_ROUTING_INFORMATION:         // We are not a downstream switch, so ignore this one.
                case CECOP_SET_STREAM_PATH:             // We are not a source, so ignore this one.
                case CECOP_SET_MENU_LANGUAGE:           // As a TV, we can ignore this message
                case CECOP_DEVICE_VENDOR_ID:
                    break;

                case CECOP_ABORT:       // Send Feature Abort for all unsupported features.
                default:

                    CecSendFeatureAbort( pCpi, CECAR_UNRECOG_OPCODE );
                    break;
            }
        }

        /* Respond to broadcast messages.   */

        else
        {
            switch ( pCpi->opcode )
            {
                case CECOP_STANDBY:

                        /* Setting this here will let the main task know    */
                        /* (via SI_CecGetPowerState) and at the same time   */
                        /* prevent us from broadcasting a STANDBY message   */
                        /* of our own when the main task responds by        */
                        /* calling SI_CecSetPowerState( STANDBY );          */

                    l_powerStatus = CEC_POWERSTATUS_STANDBY;
                    break;

                case CECOP_ACTIVE_SOURCE:
                    CecHandleActiveSource( pCpi );
                    break;

                case CECOP_REPORT_PHYSICAL_ADDRESS:
                    CecHandleReportPhysicalAddress( pCpi );
                    break;

                /* Do not reply to 'Broadcast' msgs that we don't need.  */

                case CECOP_REQUEST_ACTIVE_SOURCE:       // We are not a source, so ignore this one.
                	SI_StartActiveSource(0,0);//2011.08.03
					break;
                case CECOP_ROUTING_CHANGE:              // We are not a downstream switch, so ignore this one.
                case CECOP_ROUTING_INFORMATION:         // We are not a downstream switch, so ignore this one.
                case CECOP_SET_STREAM_PATH:             // We are not a source, so ignore this one.
                case CECOP_SET_MENU_LANGUAGE:           // As a TV, we can ignore this message
                    break;
            }
        }
    }

    return( TRUE );
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// All public API functions are below
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Function:    si_CecSendMessage
// Description: Send a single byte (no parameter) message on the CEC bus.  Does
//              no wait for a reply.
//------------------------------------------------------------------------------

void si_CecSendMessage ( byte opCode, byte dest )
{
    SI_CpiData_t cecFrame;

    cecFrame.opcode        = opCode;
    cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, dest );
    cecFrame.argCount      = 0;
    SI_CpiWrite( &cecFrame );
}

#ifdef CEC_KeyTransfer
//------------------------------------------------------------------------------
// Function:    SI_CecSendUserControlPressed
// Description: Send a remote control key pressed command to the
//              current active source.
//------------------------------------------------------------------------------

void SI_CecSendUserControlPressed ( byte keyCode )
{
    if ( !l_cecEnabled )
        return;

    if ( l_activeSrcLogical != CEC_LOGADDR_UNREGORBC )
    {
        CecSendUserControlPressed( keyCode, l_activeSrcLogical );
    }
}

//------------------------------------------------------------------------------
// Function:    SI_CecSendUserControlReleased
// Description: Send a remote control key released command to the
//              current active source.
//------------------------------------------------------------------------------

void SI_CecSendUserControlReleased ( void )
{
    if ( !l_cecEnabled )
        return;

    if ( l_activeSrcLogical != CEC_LOGADDR_UNREGORBC )
    {
        si_CecSendMessage( CECOP_USER_CONTROL_RELEASED, l_activeSrcLogical );
    }
}
#endif

//2011.08.03 Send Image view on, Active Source Begin
void SI_CecSendImageViewOn(void)
{
	//send image view on first
	si_CecSendMessage(CECOP_IMAGE_VIEW_ON,CEC_LOGADDR_TV);
	
}
void SI_CecSendActiveSource(void)
{
	SI_CpiData_t    cecFrame;

	cecFrame.opcode        = CECOP_ACTIVE_SOURCE;
	cecFrame.srcDestAddr   = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC);
	cecFrame.args[0]       = (SI_CecGetDevicePA()&0xFF00)>>8;             // [Physical Address]
	cecFrame.args[1]       = (SI_CecGetDevicePA()&0x00FF);             // [Physical Address]
	cecFrame.argCount      = 2;
	SI_CpiWrite( &cecFrame );
}
//2011.08.03 Send Image view on, Active Source End
//------------------------------------------------------------------------------
// Function:    SI_CecGetPowerState
// Description: Returns the CEC local power state.
//              Should be called after every call to SI_CecHandler since
//              a CEC device may have issued a standby or view message.
//
// Valid values:    CEC_POWERSTATUS_ON
//                  CEC_POWERSTATUS_STANDBY
//                  CEC_POWERSTATUS_STANDBY_TO_ON
//                  CEC_POWERSTATUS_ON_TO_STANDBY
//------------------------------------------------------------------------------

//byte SI_CecGetPowerState ( void )
//{
//
//    return( l_powerStatus );
//}

//------------------------------------------------------------------------------
// Function:    SI_CecSetPowerState
// Description: Set the CEC local power state.
//
// Valid values:    CEC_POWERSTATUS_ON
//                  CEC_POWERSTATUS_STANDBY
//                  CEC_POWERSTATUS_STANDBY_TO_ON
//                  CEC_POWERSTATUS_ON_TO_STANDBY
//------------------------------------------------------------------------------

//2011.08.03 delete
//void SI_CecSetPowerState ( byte newPowerState )
//{
//    if ( !l_cecEnabled )
//        return;
//
//    if ( l_powerStatus != newPowerState )
//    {
//        switch ( newPowerState )
//        {
//            case CEC_POWERSTATUS_ON:
//
//                /* Find out who is the active source. Let the   */
//                /* ACTIVE_SOURCE handler do the rest.           */
//
//                si_CecSendMessage( CECOP_REQUEST_ACTIVE_SOURCE, CEC_LOGADDR_UNREGORBC );
//                break;
//
//            case CEC_POWERSTATUS_STANDBY:
//
//                /* If we are shutting down, tell every one else to also.   */
//
//                si_CecSendMessage( CECOP_STANDBY, CEC_LOGADDR_UNREGORBC );
//                break;
//
//            case CEC_POWERSTATUS_STANDBY_TO_ON:
//            case CEC_POWERSTATUS_ON_TO_STANDBY:
//            default:
//                break;
//        }
//
//    l_powerStatus = newPowerState;
//    }
//}

//------------------------------------------------------------------------------
// Function:    SI_CecSourceRemoved
// Description: The hardware detected an HDMI connector removal, so clear the
//              position in our device list.
//
// TODO:    The source that was removed may or may not have had time to send out
//          an INACTIVE_SOURCE message, so we may want to emulate that action.
//
//------------------------------------------------------------------------------

//void SI_CecSourceRemoved ( byte portIndex )
//{
//    if ( !l_cecEnabled )
//        return;
//
//    CPI_DEBUG_PRINT(
//        
//        ( "CEC Source removed: Port: %d, LA:%02X \n",
//        (int)portIndex, (int)g_childPortList[ portIndex].cecLA
//        ));
//}

//------------------------------------------------------------------------------
// Function:    SI_CecGetDevicePA
// Description: Return the physical address for this Host device
//
//------------------------------------------------------------------------------

word SI_CecGetDevicePA ( void )
{
    return( g_cecPhysical );
}

//------------------------------------------------------------------------------
// Function:    SI_CecSetDevicePA
// Description: Set the host device physical address (initiator physical address)
//------------------------------------------------------------------------------

void SI_CecSetDevicePA ( word devPa )
{
    g_cecPhysical = devPa;
    CPI_DEBUG_PRINT(  ("\nDevice PA [%04X]\n", (int)devPa ));
}

//------------------------------------------------------------------------------
// Function:    SI_CecInit
// Description: Initialize the CEC subsystem.
//------------------------------------------------------------------------------

bool SI_CecInit ( void )
{
    word i;
	TPI_TRACE_PRINT(("\n>>SI_CecInit()\n"));

    SI_CpiInit();

	for ( i = 0; i < SII_NUMBER_OF_PORTS; i++)
  	  {
        //if this device is the TV then assign the PA to
        //the childPortList
        if (SI_CpiGetLogicalAddr() == CEC_LOGADDR_TV)
            g_childPortList[i].cecPA = (i+1) << 12;
        else
            g_childPortList[i].cecPA = 0xFFFF;
        g_childPortList[i].cecLA = 0xFF;
   	 }


    l_cecEnabled = TRUE;
    return( TRUE );
}

//------------------------------------------------------------------------------
// Function:    si_TaskServer
// Description: Calls the current task handler.  A task is used to handle cases
//              where we must send and receive a specific set of CEC commands.
//------------------------------------------------------------------------------

static void si_TaskServer ( SI_CpiStatus_t *pCecStatus )
{
    switch ( g_currentTask )
    {
        case SI_CECTASK_IDLE:
            if ( l_newTask )
            {
                /* New task; copy the queued task block into the operating task block.  */

                memcpy( &l_cecTaskState, &l_cecTaskStateQueued, sizeof( CEC_TASKSTATE ));
                l_newTask = FALSE;
                g_currentTask = l_cecTaskState.task;
            }
            break;

        case SI_CECTASK_ENUMERATE:
            g_currentTask = CecTaskEnumerate( pCecStatus );
            break;

        //2011.08.03 logical address  begin
        //case SI_CECTASK_NEWSOURCE:
            //g_currentTask = CecTaskStartNewSource( pCecStatus );
            //break;
	case SI_CECTASK_StartActive:
			CPI_DEBUG_PRINT(  ( "si_TaskServer StartActive1  g_currentTask=%x\n",(int)g_currentTask ));
			g_currentTask = CECTaskStartActive( pCecStatus );
			CPI_DEBUG_PRINT(  ( "si_TaskServer StartActive2  g_currentTask=%x\n",(int)g_currentTask ));
			break;
        //2011.08.03 logical address  End

        default:
            break;
    }

}

//------------------------------------------------------------------------------
// Function:    SI_CecSwitchSources
// Description: Send the appropriate CEC commands for switching from the current
//              active source to the requested source.
//              This is called after the main process has ALREADY performed the
//              actual port switch on the RX, so the physical address is changed
//              even if the input is NOT in the CEC list, either because it has
//              not reported itself as an active source, or it is not CEC
//              capable.
//------------------------------------------------------------------------------

//bool SI_CecSwitchSources ( byte portIndex )
//{
//    byte         newLogical;
//    word        newPhysical = g_cecPhysical;
//    SI_CpiData_t    cecFrame;
//    bool            success = FALSE;
//
//    while ( l_cecEnabled )
//    {
//        newPhysical = g_childPortList[portIndex].cecPA;
//        if ( newPhysical != 0xFFFF )
//        {
//            newLogical = g_childPortList[portIndex].cecLA;
//
//                /* Broadcast a routing change message to tell switches what's going on.     */
//                /* This happens even if this is a non-CEC device (CEC_LOGADDR_UNREGORBC).   */
//
//            cecFrame.opcode         = CECOP_ROUTING_CHANGE;
//            cecFrame.srcDestAddr    = MAKE_SRCDEST( g_cecAddress, CEC_LOGADDR_UNREGORBC );
//            cecFrame.args[0]        = (byte)(l_activeSrcPhysical >> 8);
//            cecFrame.args[1]        = (byte)l_activeSrcPhysical;
//            cecFrame.args[2]        = (byte)(newPhysical >> 8);
//            cecFrame.args[3]        = (byte)newPhysical;
//            cecFrame.argCount       = 4;
//            l_activeSrcLogical  = newLogical;
//            SI_CpiWrite( &cecFrame );
//        }
//        else
//        {
//            CPI_DEBUG_PRINT( ( "\nCecSwitchSources: Port %d not found in CEC list\n", (int)portIndex ));
//
//                /* If not in the CEC list, we still have to change the physical */
//                /* address, and we change the logical address to 'UNKNOWN'.     */
//
//            l_activeSrcLogical  = CEC_LOGADDR_UNREGORBC;
//            break;
//        }
//
//        l_activeSrcPhysical = newPhysical;
//
//        CPI_DEBUG_PRINT( ( "\nCecSwitchSources: %02X to %02X (%04X)\n", (int)l_activeSrcLogical, (int)newLogical, newPhysical ));
//
//            /* Start a task to handshake with the new source about power    */
//            /* status and playback.                                         */
//
//        l_cecTaskStateQueued.task       = SI_CECTASK_NEWSOURCE;
//        l_cecTaskStateQueued.state      = NST_START;
//        l_cecTaskStateQueued.destLA     = newLogical;
//        l_cecTaskStateQueued.cpiState   = CPI_IDLE;
//        l_cecTaskStateQueued.taskData1  = 0;
//        l_cecTaskStateQueued.taskData2  = newPhysical;
//        l_newTask = TRUE;
//
//        success = TRUE;
//        break;
//    }
//
//    return( success );
//}

//------------------------------------------------------------------------------
// Function:    SI_CecEnumerate
// Description: Send the appropriate CEC commands to enumerate all the logical
//              devices on the CEC bus.
//------------------------------------------------------------------------------

//bool SI_CecEnumerate ( void )
//{
//    bool            success = FALSE;
//
//    /* If the task handler queue is not full, add the enumerate task.   */
//
//    if ( !l_newTask )
//    {
//        l_cecTaskStateQueued.task       = SI_CECTASK_ENUMERATE;
//        l_cecTaskStateQueued.state      = 0;
//        l_cecTaskStateQueued.destLA     = 0;
//        l_cecTaskStateQueued.cpiState   = CPI_IDLE;
//        l_cecTaskStateQueued.taskData1  = 0;
//        l_cecTaskStateQueued.taskData2  = 0;
//        l_newTask   = TRUE;
//        success     = TRUE;
//    }
//
//    return( success );
//}
//2011.08.03 logical address Begin
bool SI_CecEnumerate ( byte nexttask1,byte nexttask2 )
{
	/* If the task handler queue is not full, add the enumerate task.   */

	//if ( !l_newTask )
	{
		CECLogicTable = 0;
		
		l_cecTaskState.task       = SI_CECTASK_ENUMERATE;
		l_cecTaskState.state      = 0;
		l_cecTaskState.destLA     = abEnumTblDVD[CECLogicTable+1];
		l_cecTaskState.cpiState   = CPI_WAIT_ACK;
		l_cecTaskState.taskData1  = nexttask1;
		l_cecTaskState.taskData2  = nexttask2;
		//l_newTask   = TRUE;

		SI_CpiSetLogicalAddr( l_cecTaskState.destLA );
		
		SI_CpiSendPing( l_cecTaskState.destLA );
		CPI_DEBUG_PRINT(  ( "Begin...CEC Enumerate...\n" ));

		g_currentTask = SI_CECTASK_ENUMERATE;
	}
	
	//l_cecTaskState.destLA = abEnumTblDVD[CECPingCount+1];
	//CPI_DEBUG_PRINT(  ( "...Ping...%x\n" ,(int)( l_cecTaskState.destLA )));
	//l_cecTaskState.cpiState = CPI_WAIT_ACK;

	return 1;
		
//    bool            success = FALSE;
//
//    /* If the task handler queue is not full, add the enumerate task.   */
//
//    if ( !l_newTask )
//    {
//        l_cecTaskStateQueued.task       = SI_CECTASK_ENUMERATE;
//        l_cecTaskStateQueued.state      = 0;
//        l_cecTaskStateQueued.destLA     = 0;
//        l_cecTaskStateQueued.cpiState   = CPI_IDLE;
//        l_cecTaskStateQueued.taskData1  = 0;
//        l_cecTaskStateQueued.taskData2  = 0;
//        l_newTask   = TRUE;
//        success     = TRUE;
//    }
//
//    return( success );
}

void SI_StartActiveSource ( byte nexttask1, byte nexttask2 )
{
	
	l_cecTaskState.task       = SI_CECTASK_StartActive;
	l_cecTaskState.state      = 0;
	l_cecTaskState.destLA     = 0;
	l_cecTaskState.cpiState   = CPI_WAIT_ACK;
	l_cecTaskState.taskData1  = nexttask1;
	l_cecTaskState.taskData2  = nexttask2;

	g_currentTask = SI_CECTASK_StartActive;

	SI_CecSendImageViewOn();
	//SI_CecSendActiveSource();
}

//2011.08.03 logical address End

//------------------------------------------------------------------------------
// Function:    SI_CecHandler
// Description: Polls the send/receive state of the CPI hardware and runs the
//              current task, if any.
//
//              A task is used to handle cases where we must send and receive
//              a specific set of CEC commands.
//------------------------------------------------------------------------------

byte SI_CecHandler ( byte currentPort, bool returnTask )
{
    SI_CpiStatus_t  cecStatus;

    //l_portSelect = currentPort;

    /* Get the CEC transceiver status and pass it to the    */
    /* current task.  The task will send any required       */
    /* CEC commands.                                        */
    SI_CpiStatus( &cecStatus );
    si_TaskServer( &cecStatus );

    /* Now look to see if any CEC commands were received.   */

    if ( cecStatus.rxState )
    {
        byte         frameCount;
        SI_CpiData_t    cecFrame;

        /* Get CEC frames until no more are present.    */

        cecStatus.rxState = 0;      // Clear activity flag
        while (frameCount = ((SiIRegioRead( REG_CEC_RX_COUNT) & 0xF0) >> 4))
        {
            CPI_DEBUG_PRINT( ("\n%d frames in Rx Fifo\n", (int)frameCount ));

            if ( SI_CpiRead( &cecFrame ))
            {
                CPI_DEBUG_PRINT(  ( "Error in Rx Fifo \n" ));
                break;
            }

            /* Send the frame through the RX message Handler chain. */

            for (;;)
            {
//                if ( si_CecRxMsgHandlerFirst( &cecFrame ))  // We get a chance at it before the end user App
//                    break;
//#ifdef SII_ARC_SUPPORT
//                if ( si_CecRxMsgHandlerARC( &cecFrame ))    // A captive end-user handler to check for HEAC commands
//                    break;
//#endif
//                if ( CpCecRxMsgHandler( &cecFrame ))        // Let the end-user app have a shot at it.
//                    break;

                si_CecRxMsgHandlerLast( &cecFrame );        // We get a chance at it afer the end user App to cleanup
                break;
            }
        }
    }

    //if ( l_portSelect != currentPort )
    //{
    //    CPI_DEBUG_PRINT( ("\nNew port value returned: %02X (Return Task: %d)\n", (int)l_portSelect, (int)returnTask ));
    //}
    return( returnTask ? g_currentTask : l_portSelect );
}
#endif

