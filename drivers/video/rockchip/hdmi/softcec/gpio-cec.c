#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <mach/gpio.h>
#include "gpio-cec.h"

static GPIO_Cec_t GPIOCec;

static void GPIOCecWorkFunc(struct work_struct *work);
static void GPIO_CecStartRead(void);

static char LA_Player[3] = { CEC_LOGADDR_PLAYBACK1, CEC_LOGADDR_PLAYBACK2, CEC_LOGADDR_PLAYBACK3 };

static inline unsigned int GPIO_CecComputePeriod(struct timeval *pre_time, struct timeval *cur_time)
{
	if(pre_time->tv_sec == 0 && pre_time->tv_usec == 0)
		return 0;
	else
		return (cur_time->tv_sec - pre_time->tv_sec)*1000000 + (cur_time->tv_usec - pre_time->tv_usec);
}

static void GPIO_CecSubmitWork(int event, int delay, void *data)
{
	struct GPIO_Cec_delayed_work *work;

//	DBG("%s event %04x delay %d", __FUNCTION__, event, delay);
	
	work = kmalloc(sizeof(struct GPIO_Cec_delayed_work), GFP_ATOMIC);

	if (work) {
		INIT_DELAYED_WORK(&work->work, GPIOCecWorkFunc);
		work->event = event;
		work->data = data;
		queue_delayed_work(GPIOCec.workqueue,
				   &work->work,
				   msecs_to_jiffies(delay));
	} else {
		CECDBG(KERN_WARNING "GPIO CEC: Cannot allocate memory to "
				    "create work\n");;
	}
}

static void cec_send_bit(char bit)
{

#if 1
	gpio_set_value(GPIOCec.gpio, GPIO_LOW);
	if(bit == BIT_0)
		usleep_range(BIT_0_LOWLEVEL_PERIOD_MIN, BIT_0_LOWLEVEL_PERIOD_MAX);
	else if(bit == BIT_1)
		usleep_range(BIT_1_LOWLEVEL_PERIOD_MIN, BIT_1_LOWLEVEL_PERIOD_MAX);
	else
		usleep_range(BIT_START_LOWLEVEL_PERIOD_MIN, BIT_START_LOWLEVEL_PERIOD_MAX);
	gpio_set_value(GPIOCec.gpio, GPIO_HIGH);
	if(bit == BIT_0)
		usleep_range(BIT_0_HIGHLEVEL_PERIOD_MIN - BIT_0_LOWLEVEL_PERIOD_MIN, BIT_0_HIGHLEVEL_PERIOD_MAX - BIT_0_LOWLEVEL_PERIOD_MAX);
	else if(bit == BIT_1)
		usleep_range(BIT_1_HIGHLEVEL_PERIOD_MIN - BIT_1_LOWLEVEL_PERIOD_MIN, BIT_1_HIGHLEVEL_PERIOD_MAX - BIT_1_LOWLEVEL_PERIOD_MAX);
	else
		usleep_range(BIT_START_HIGHLEVEL_PERIOD_MIN - BIT_START_LOWLEVEL_PERIOD_MIN, BIT_START_HIGHLEVEL_PERIOD_MAX - BIT_START_LOWLEVEL_PERIOD_MAX);
#else
	struct timeval  ts_start, ts_end;
	unsigned int time = 0 ,count = 0, last_time;
	
	gpio_set_value(GPIOCec.gpio, GPIO_LOW);
	do_gettimeofday(&ts_start);
	while(1) {
		do_gettimeofday(&ts_end);
		last_time = time;
		time = GPIO_CecComputePeriod(&ts_start, &ts_end);
		count++;
		if( (( bit == BIT_1) && (time > BIT_1_LOWLEVEL_PERIOD_NOR)) || 
			(( bit == BIT_0) && (time > BIT_0_LOWLEVEL_PERIOD_NOR)) ||
			(( bit == BIT_START) && (time > BIT_START_LOWLEVEL_PERIOD_NOR)) )
		{
//			CECDBG(" 0 level is %d %d lasttiem %d\n", time, count, last_time);
			break;
		}
	}
	gpio_set_value(GPIOCec.gpio, GPIO_HIGH);
	count = 0;
	while(1) {
		do_gettimeofday(&ts_end);
		last_time = time;
		time = GPIO_CecComputePeriod(&ts_start, &ts_end);
		count++;
		if( (( bit == BIT_1) && (time > BIT_1_HIGHLEVEL_PERIOD_NOR)) || 
			(( bit == BIT_0) && (time > BIT_0_HIGHLEVEL_PERIOD_NOR)) ||
			(( bit == BIT_START) && (time > BIT_START_HIGHLEVEL_PERIOD_NOR)))
		{
//			CECDBG(" 1 level is %d %d lastime %d\n", time, count, last_time);
			break;
		}
	}
#endif
}

static int cec_read_ack(void)
{
	struct timeval  ts_start, ts_end;
	unsigned int time;
	
	do_gettimeofday(&ts_start);
	gpio_set_value(GPIOCec.gpio, GPIO_LOW);
	udelay(600);
	gpio_direction_input(GPIOCec.gpio);
	udelay(250);
	while(gpio_get_value(GPIOCec.gpio) == 0);
	do_gettimeofday(&ts_end);
	time = GPIO_CecComputePeriod(&ts_start, &ts_end);
//	CECDBG("ack low level time is %d\n", time);
	if(time > 1300 && time < 1700) {
		udelay(900);
		return 0;
	}
	else {
		udelay(1800);
		return 1;
	}
}

static int GPIO_CecSendByte(char value, char EOM)
{
	int i;
//	CECDBG("%s %02x %d\n", __FUNCTION__, value, EOM);
	gpio_direction_output(GPIOCec.gpio, GPIO_HIGH);
	for(i = 7; i >= 0; i--) {
		cec_send_bit((value >> i) & 0x1);
	}
	cec_send_bit(EOM);
	return cec_read_ack();
}

static int GPIO_CecSendFrame(CEC_FrameData_t *Frame)
{
	int i, ack, trytimes = 5, isDirectAddressed = !((Frame->srcDestAddr & 0x0F ) == CEC_LOGADDR_UNREGORBC );;
	
	CECDBG("TX srcDestAddr %02x opcode %02x ",
		 Frame->srcDestAddr, Frame->opcode);
	if(Frame->argCount) {
		CECDBG("args:");
		for(i = 0; i < Frame->argCount; i++) {
			CECDBG("%02x ", Frame->args[i]);
		}
	}
	CECDBG("\n");
	disable_irq(GPIOCec.irq);
	free_irq(GPIOCec.irq, NULL);
start:	
	if(trytimes == 0)
		goto out;
	trytimes--;
	msleep(17);
	cec_send_bit(BIT_START);
	ack = GPIO_CecSendByte(Frame->srcDestAddr, 0);
	if(isDirectAddressed && (ack == 1) ) goto start;
	if(Frame->argCount == 0)
		ack = GPIO_CecSendByte(Frame->opcode, 1);
	else
		ack = GPIO_CecSendByte(Frame->opcode, 0);
	if(isDirectAddressed && (ack == 1) ) goto start;
	for(i = 0; i < Frame->argCount; i++) {
		if(i == (Frame->argCount - 1))
			ack = GPIO_CecSendByte(Frame->args[i], 1);
		else
			ack = GPIO_CecSendByte(Frame->args[i], 0);
		if(isDirectAddressed && (ack == 1) ) goto start;
	}
out:
	GPIO_CecStartRead();
	return ack;
}

static int GPIO_Cec_Ping(char LA)
{
	int ack = 1, trytimes = 5;
	CECDBG("Ping LA %02x\n", LA);
	disable_irq(GPIOCec.irq);
	free_irq(GPIOCec.irq, NULL);
	while(ack == 1) {
		msleep(17);
		cec_send_bit(BIT_START);
		ack = GPIO_CecSendByte(LA << 4 | LA, 1);
		CECDBG(" ack is %d\n", ack);
		trytimes--;
		if(trytimes == 0)
			break;
	}
	GPIO_CecStartRead();
	return ack;
}

static int CecSendMessage ( char opCode, char dest )
{
	CEC_FrameData_t cecFrame;

    cecFrame.opcode        = opCode;
    cecFrame.srcDestAddr   = MAKE_SRCDEST( GPIOCec.address_logic, dest );
    cecFrame.argCount      = 0;
    return GPIO_CecSendFrame( &cecFrame );
}

static void CecSendFeatureAbort ( CEC_FrameData_t *pCpi, char reason )
{
    CEC_FrameData_t cecFrame;

    if (( pCpi->srcDestAddr & 0x0F) != CEC_LOGADDR_UNREGORBC )
    {
        cecFrame.opcode        = CECOP_FEATURE_ABORT;
        cecFrame.srcDestAddr   = MAKE_SRCDEST( GPIOCec.address_logic, (pCpi->srcDestAddr & 0xF0) >> 4 );
        cecFrame.args[0]       = pCpi->opcode;
        cecFrame.args[1]       = reason;
        cecFrame.argCount      = 2;
        GPIO_CecSendFrame( &cecFrame );
    }
}

static void CecHandleInactiveSource ( CEC_FrameData_t *pCpi )
{
//    byte la;
//
//    la = (pCpi->srcDestAddr >> 4) & 0x0F;
//    if ( la == l_activeSrcLogical )    // The active source has deserted us!
//    {
//        l_activeSrcLogical  = CEC_LOGADDR_TV;
//        l_activeSrcPhysical = 0x0000;
//    }
//
//    l_portSelect = 0xFF;    // Tell main process to choose another port.
}

static void CecHandleFeatureAbort( CEC_FrameData_t *pCpi )
{
   
}

static void CecSendActiveSource(void)
{
	CEC_FrameData_t    cecFrame;

	cecFrame.opcode        = CECOP_ACTIVE_SOURCE;
	cecFrame.srcDestAddr   = MAKE_SRCDEST( GPIOCec.address_logic, CEC_LOGADDR_UNREGORBC);
	cecFrame.args[0]       = (GPIOCec.address_phy & 0xFF00) >> 8;        // [Physical Address]
	cecFrame.args[1]       = (GPIOCec.address_phy & 0x00FF);             // [Physical Address]
	cecFrame.argCount      = 2;
	GPIO_CecSendFrame( &cecFrame );
}

static void GPIO_StartActiveSource(void)
{
	int i;
	
	// GPIO simulate CEC timing may be not correct, so we try more times.
	//send image view on first
	for(i = 0; i < 1; i++) {
		if(CecSendMessage(CECOP_IMAGE_VIEW_ON,CEC_LOGADDR_TV) == 0) {
			CecSendActiveSource();
		}
	}
}

static int GPIO_Cec_Enumerate(void)
{
	int i;
	
	for(i = 0; i < 3; i++) {
		if(GPIO_Cec_Ping(LA_Player[i]) == 1) {
			GPIOCec.address_logic = LA_Player[i];
			break;
		}
	}
	if(i == 3)
		return -1;
	// Broadcast our physical address.
//	GPIO_CecSendMessage(CECOP_GET_MENU_LANGUAGE,CEC_LOGADDR_TV);
	msleep(100);
	GPIO_StartActiveSource();
	return 0;
}

static void GPIO_CecSendRXACK(void)
{
	disable_irq(GPIOCec.irq);
	free_irq(GPIOCec.irq, NULL);
	gpio_direction_output(GPIOCec.gpio, GPIO_LOW);
	udelay(1300);
	gpio_direction_input(GPIOCec.gpio);
	GPIO_CecStartRead();
}

static bool ValidateCecMessage ( CEC_FrameData_t *pCpi )
{
    char parameterCount = 0;
    bool    countOK = true;

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
        countOK = false;
    }

    return( countOK );
}

static bool GPIO_CecRxMsgHandlerLast ( CEC_FrameData_t *pCpi )
{
    bool				isDirectAddressed;
    CEC_FrameData_t		cecFrame;

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

                    GPIOCec.powerstatus = CEC_POWERSTATUS_STANDBY;
                    break;

                case CECOP_INACTIVE_SOURCE:
                    CecHandleInactiveSource( pCpi );
                    break;

                case CECOP_GIVE_PHYSICAL_ADDRESS:

                    /* TV responds by broadcasting its Physical Address: 0.0.0.0   */

                    cecFrame.opcode        = CECOP_REPORT_PHYSICAL_ADDRESS;
                    cecFrame.srcDestAddr   = MAKE_SRCDEST( GPIOCec.address_logic, CEC_LOGADDR_UNREGORBC );
                    cecFrame.args[0]       = (GPIOCec.address_phy&0xFF00)>>8;             // [Physical Address]
                    cecFrame.args[1]       = (GPIOCec.address_phy&0x00FF);             // [Physical Address]
                    cecFrame.args[2]       = CEC_LOGADDR_PLAYBACK1;//2011.08.03 CEC_LOGADDR_TV;   // [Device Type] = 0 = TV
                    cecFrame.argCount      = 3;
                    GPIO_CecSendFrame( &cecFrame );
                    break;

                case CECOP_GIVE_DEVICE_POWER_STATUS:

                    /* TV responds with power status.   */

                    cecFrame.opcode        = CECOP_REPORT_POWER_STATUS;
                    cecFrame.srcDestAddr   = MAKE_SRCDEST( GPIOCec.address_logic, (pCpi->srcDestAddr & 0xF0) >> 4 );
                    cecFrame.args[0]       = GPIOCec.powerstatus;
                    cecFrame.argCount      = 1;
                    GPIO_CecSendFrame( &cecFrame );
                    break;

                case CECOP_GET_MENU_LANGUAGE:

                    /* TV Responds with a Set Menu language command.    */

                    cecFrame.opcode         = CECOP_SET_MENU_LANGUAGE;
                    cecFrame.srcDestAddr    = CEC_LOGADDR_UNREGORBC;
                    cecFrame.args[0]        = 'e';     // [language code see iso/fdis 639-2]
                    cecFrame.args[1]        = 'n';     // [language code see iso/fdis 639-2]
                    cecFrame.args[2]        = 'g';     // [language code see iso/fdis 639-2]
                    cecFrame.argCount       = 3;
                    GPIO_CecSendFrame( &cecFrame );
                    break;

                case CECOP_GET_CEC_VERSION:

                    /* TV responds to this request with it's CEC version support.   */

                    cecFrame.srcDestAddr   = MAKE_SRCDEST( GPIOCec.address_logic, (pCpi->srcDestAddr & 0xF0) >> 4 );
                    cecFrame.opcode        = CECOP_CEC_VERSION;
                    cecFrame.args[0]       = 0x04;       // Report CEC1.3a
                    cecFrame.argCount      = 1;
                    GPIO_CecSendFrame( &cecFrame );
                    break;

                case CECOP_REPORT_POWER_STATUS:         // Someone sent us their power state.

//                    l_sourcePowerStatus = pCpi->args[0];
//
//                        /* Let NEW SOURCE task know about it.   */
//
//                    if ( l_cecTaskState.task == SI_CECTASK_NEWSOURCE )
//                    {
//                        l_cecTaskState.cpiState = CPI_RESPONSE;
//                    }
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

                    GPIOCec.powerstatus = CEC_POWERSTATUS_STANDBY;
                    break;

                case CECOP_ACTIVE_SOURCE:
//                    CecHandleActiveSource( pCpi );
                    break;

                case CECOP_REPORT_PHYSICAL_ADDRESS:
//                    CecHandleReportPhysicalAddress( pCpi );
                    break;

                /* Do not reply to 'Broadcast' msgs that we don't need.  */

                case CECOP_REQUEST_ACTIVE_SOURCE:       // We are not a source, so ignore this one.
//                	SI_StartActiveSource(0,0);//2011.08.03
					break;
                case CECOP_ROUTING_CHANGE:              // We are not a downstream switch, so ignore this one.
                case CECOP_ROUTING_INFORMATION:         // We are not a downstream switch, so ignore this one.
                case CECOP_SET_STREAM_PATH:             // We are not a source, so ignore this one.
                case CECOP_SET_MENU_LANGUAGE:           // As a TV, we can ignore this message
                    break;
            }
        }
    }

    return 0;
}

static void GPIOCecWorkFunc(struct work_struct *work)
{
	struct GPIO_Cec_delayed_work *cec_w =
		container_of(work, struct GPIO_Cec_delayed_work, work.work);
	CEC_FrameData_t cecFrame;
	int i;
	
	switch(cec_w->event)
	{
		case EVENT_RX_SENDACK:
			GPIO_CecSendRXACK();
			break;
		case EVENT_ENUMERATE:
			GPIO_Cec_Enumerate();
			break;
		case EVENT_RX_FRAME:
			memset(&cecFrame, 0, sizeof(CEC_FrameData_t));
			cecFrame.srcDestAddr = GPIOCec.RxFiFo[0];
			CECDBG("RX srcDestAddr %02x ", cecFrame.srcDestAddr);
			if(GPIOCec.RxCount > 1) {
				cecFrame.opcode = GPIOCec.RxFiFo[1];
				CECDBG("opcode %02x ", cecFrame.opcode);
				cecFrame.argCount = GPIOCec.RxCount - 2;
				if(cecFrame.argCount) {
					CECDBG("args: ");
					for(i = 0; i < cecFrame.argCount; i++) {
						cecFrame.args[i] = GPIOCec.RxFiFo[i + 2];
						CECDBG("%02x ", cecFrame.args[i]);
					}
				}
			}
			CECDBG("\n");
			GPIO_CecRxMsgHandlerLast(&cecFrame);
			break;
		default:
			break;
	}
	
	if(cec_w->data)
		kfree(cec_w->data);
	kfree(cec_w);
}

static int GPIO_CecDetectBit(unsigned lowlevel, unsigned highlevel)
{
	if( lowlevel > BIT_START_LOWLEVEL_PERIOD_MIN && 
		lowlevel < BIT_START_LOWLEVEL_PERIOD_MAX &&
		highlevel > BIT_START_HIGHLEVEL_PERIOD_MIN &&
		highlevel < BIT_START_HIGHLEVEL_PERIOD_MAX )
		return BIT_START;
	else if (
		lowlevel > BIT_0_LOWLEVEL_PERIOD_MIN && 
		lowlevel < BIT_0_LOWLEVEL_PERIOD_MAX &&
		highlevel > BIT_0_HIGHLEVEL_PERIOD_MIN &&
		highlevel < BIT_0_HIGHLEVEL_PERIOD_MAX
		)
		return BIT_0;
	else if (
		lowlevel > BIT_1_LOWLEVEL_PERIOD_MIN && 
		lowlevel < BIT_1_LOWLEVEL_PERIOD_MAX &&
		highlevel > BIT_1_HIGHLEVEL_PERIOD_MIN &&
		highlevel < BIT_1_HIGHLEVEL_PERIOD_MAX
		)
		return BIT_1;
	else
		return BIT_UNKOWN;
}

static irqreturn_t GPIO_CecDetectIrq(int irq, void *dev_id)
{
	char rxbit;
	struct timeval  ts;
	unsigned int period;
	do_gettimeofday(&ts);
	period = GPIO_CecComputePeriod(&GPIOCec.time_pre, &ts);
	if(GPIOCec.trigger == IRQF_TRIGGER_FALLING) {
		GPIOCec.period_highlevel = period;
		GPIOCec.time_pre = ts;
		GPIOCec.trigger = IRQF_TRIGGER_RISING;
	}
	else {
		GPIOCec.period_lowlevel = period;
		GPIOCec.trigger = IRQF_TRIGGER_FALLING;
	}
	irq_set_irq_type(GPIOCec.irq, GPIOCec.trigger);
	if(GPIOCec.period_highlevel && GPIOCec.period_lowlevel) {
		rxbit = GPIO_CecDetectBit(GPIOCec.period_lowlevel, GPIOCec.period_highlevel);
//		CECDBG("low %uus high %uus bit is %d\n", GPIOCec.period_lowlevel, GPIOCec.period_highlevel, rxbit);		
		if(GPIOCec.state == RX_RECEIVING) {
			GPIOCec.rxbitcount++;
			if(GPIOCec.rxbitcount < 9) {
				if(rxbit == BIT_0 || rxbit == BIT_1)
					GPIOCec.rxdata |= rxbit << (8 - GPIOCec.rxbitcount);
				else {
					CECDBG(KERN_ERR "recevied incorrect bit %d no%d low %d high %d\n", 
						rxbit, GPIOCec.rxbitcount, GPIOCec.period_lowlevel, GPIOCec.period_highlevel);
    				GPIOCec.rxdata = 0;
    				GPIOCec.rxbitcount = 0;
    				GPIOCec.state = RX_LISTENING;
				}
			}
			else if(GPIOCec.rxbitcount == 9) {
//				CECDBG("Rx Data %02x\n", GPIOCec.rxdata);
				GPIOCec.RxFiFo[GPIOCec.RxCount++] = GPIOCec.rxdata;
				GPIOCec.rxdata = 0;
				GPIOCec.period_lowlevel = 0;
    			GPIOCec.period_highlevel = 0;
				if((GPIOCec.RxFiFo[0] & 0x0F) == GPIOCec.address_logic) {
//					CECDBG("Need to Send ACK %d\n", GPIOCec.trigger);
					GPIO_CecSubmitWork(EVENT_RX_SENDACK, 0, NULL);
//					return IRQ_HANDLED;
				}
				if(rxbit == 1) {
//					CECDBG("it is last frame data\n");
					GPIOCec.state = RX_LISTENING;
					GPIOCec.rxbitcount = 0;
					GPIO_CecSubmitWork(EVENT_RX_FRAME, 0, NULL);
				}
//				else
//					CECDBG("there is another data later\n");
			}else
				GPIOCec.rxbitcount = 0;
		}else if(rxbit == BIT_START) {
			GPIOCec.state = RX_RECEIVING;
			GPIOCec.RxCount = 0;
		}
		GPIOCec.period_highlevel = 0;
		GPIOCec.period_lowlevel = 0;
	}

    return IRQ_HANDLED;
}

static void GPIO_CecStartRead(void)
{
	int rc;
	GPIOCec.trigger = IRQF_TRIGGER_FALLING;
	GPIOCec.rxbitcount = 0;
    GPIOCec.time_pre.tv_sec = 0;
    GPIOCec.time_pre.tv_usec = 0;
    GPIOCec.period_lowlevel = 0;
    GPIOCec.period_highlevel = 0;
    gpio_direction_input(GPIOCec.gpio);
    rc = request_irq(GPIOCec.irq, GPIO_CecDetectIrq, GPIOCec.trigger, NULL, NULL);
}

void GPIO_CecEnumerate(void)
{
	GPIO_CecSubmitWork(EVENT_ENUMERATE, 0, NULL);
}

void GPIO_CecSetDevicePA(int devPa)
{
	GPIOCec.address_phy = devPa;
}

int GPIO_CecInit(int gpio)
{
	int rc;
	
	if(gpio == -1) {
		CECDBG(KERN_ERR "GPIO: Invalid GPIO\n");
		return -1;
	}
	
	rc = gpio_request(gpio, "cec gpio");
	if(rc < 0) {
        CECDBG(KERN_ERR "fail to request cec gpio\n");
        return -1;
    }
    memset(&GPIOCec, 0, sizeof(GPIO_Cec_t));
    GPIOCec.workqueue = create_singlethread_workqueue("gpio cec");
	if (GPIOCec.workqueue == NULL) {
		CECDBG(KERN_ERR "GPIO CEC: create workqueue failed.\n");
		 return -1;
	}
	
    GPIOCec.gpio = gpio;
    gpio_pull_updown(GPIOCec.gpio, PullDisable);
    
    GPIOCec.irq = gpio_to_irq(GPIOCec.gpio);
    
    GPIOCec.state = RX_LISTENING;
    GPIO_CecStartRead();
 
    return 0;
}