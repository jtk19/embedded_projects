#ifndef __INSYNC_KEYPANEL_H__
#define __INSYNC_KEYPANEL_H__


// Hardware will send me an RX frame every 50 milliseconds (to be tuned)
#define IN_KP_RX_DATA_PERIOD_MILLISEC	50	

/* The alpha-numeric keys */
#define IN_KP_KEY1		0		// ABC-1 key id
#define IN_KP_KEY2		1		// DEF-2 key id
#define IN_KP_KEY3		2		// GHI-3 key id
#define IN_KP_KEY4		3		// JKL-4 key id
#define IN_KP_KEY5		4		// MNO-5 key id
#define IN_KP_KEY6		5		// PQR-6 key id 
#define IN_KP_KEY7		6		// ST-7  key id
#define IN_KP_KEY8		7		// UV-8  key id
#define IN_KP_KEY9		8		// WX-9  key id
#define IN_KP_KEY10		9		// YZ-10 key id

/* The control side keys to the left and right of the alph-numeric key pad */
#define IN_KP_CTRL1		10		// top left control key
#define IN_KP_CTRL2		11		// bottom left control key
// top right control key unused
#define IN_KP_CTRL3		12		// bottom right control key

/* The circular direction key pad to the left */
#define IN_KP_DIR_UP		13		// the direction key curved up
#define IN_KP_DIR_DOWN		14		// the direction key curved down
#define IN_KP_DIR_RIGHT		15		// the direction key curved right -> )
#define IN_KP_DIR_LEFT		16		// the direction key curved left -> (
#define IN_KP_DIR_MIDDLE	17		// the direction key in the middle

/* The Rotary Encoder */
#define IN_KP_ROTARY_KEY	18		// the rotaryr encode as a pressable key

#define IN_KP_NUM_KEYS		19
#define IN_KP_MAX_NUM_KEYS	24		// the maximum number of keys that can be encoded
									// in the current 32 bit RX frame: 24 =>
									// 32 - 8 bits for the Rotary Encoder

/* The Rotary Encoder id as a rotating maginitude */
#define IN_KP_ROTARY_ENCODER	(IN_KP_NUM_KEYS)	// encoder key id

/* The RX frame key mask for key id n: n in [0,18]  */
#define IN_KP_KEY_MASK(n)	( ((n) < (IN_KP_NUM_KEYS)) ? (1 << (n)) : ( 0 ) )	

/* The frame content block masks */
#define IN_KP_ROTARY_ENCODER_OFFSET	24
#define IN_KP_ROTARY_ENCODER_MASK	0xFF000000	// the 8 most significant bits for the encoder	
#define IN_KP_KEY_STATES_MASK		0x0007FFFF	// the 19 least significant bits for the key states
#define IN_KP_RX_EMPTY_PADDING_MASK	0x00F80000	// 5 bits empty padding in the middle,
												// allowing for future keys
												
/* LED states in TX frame */
/* 16 LED's: 10 for the alpha-numeric keys, and 6 control LED's */
#define IN_KP_NUM_LED		16
#define IN_KP_LED(n) 		( ((n) < (IN_KP_NUM_LED)) ? (n) : ( -1 ) )			// LED id
#define IN_KP_LED_MASK(n) 	( ((n) < (IN_KP_NUM_LED)) ? (1 << (n)) : ( 0 ) )	

#define IN_KP_LED_ACTIVE_STATES_MASK	0x0000FFFF	// only 16 LED's active
#define IN_KP_LED_EMPTY_STATES_MASK		(~(IN_KP_LED_ACTIVE_STATES_MASK))


#define IN_KP_DATA_PADDING_VALUE	0x0		// any data frame padding with 0
#define	IN_KP_NUM_FRAME_SYNC_BITS	1		// 1 synchronization bit
#define IN_KP_SYNC_BIT_VBALUE		0x0		// sync bit value is 0
#define IN_KP_QUIESCENT_VALUE		0x1		// drive TX/RX with 1 when no data is being sent

#define IN_KP_RX_FRAME_LEN_BITS		32		// RX key-state & encoder data frame length
#define IN_KP_TX_FRAME_LEN_BITS		32		// TX LED-state data frame length
#define IN_KP_RX_FRAME_LEN_LESS1	0x1F	// RX frame 31+1 bits 
#define IN_KP_TX_FRAME_LEN_LESS1	0x1F	// TX frame 31+1, only 16 of them active 


/*---------------------------------------------------------------
 * Event Data for events sent into user space
 */

/* key event id's */
// up to 7 event types can be sent in the allocated 3 bits
#define IN_KP_EVENT_KEY_UP				0
#define	IN_KP_EVENT_KEY_DOWN			1
#define IN_KP_EVENT_ROTARY_RIGHT_TURN	2
#define IN_KP_EVENT_ROTARY_LEFT_TURN	3

/* The event data frame */
// 16 bits: VVVVVVVV EEEKKKKK
#define IN_KP_EVENT_KEY_ID_MASK		0x001F	// the 5 LSB to carry one of the 32 key id's: KKKKK
#define IN_KP_EVENT_EVENT_ID_MASK	0x00E0	// the next 3 bits to carry the event id: EEE
#define IN_KP_EVENT_VALUE_MASK		0xFF00	// if this is a rotary encoder event, the
											// 8 bit signed value is carried in the 8 MSB: VVVVVVVV
											// the other KEY_UP/KEY_DOWN events carry no value

#define IN_KP_EVENT_KEY_ID_OFFSET		0
#define IN_KP_EVENT_KEY_ID_SIZE			5
#define IN_KP_EVENT_EVENT_ID_OFFSET		5
#define IN_KP_EVENT_EVENT_ID_SIZE		3
#define IN_KP_EVENT_ROTARY_VALUE_OFFSET	8
#define IN_KP_EVENT_ROTARY_VALUE_SIZE	8


/*---------------------------------------------------------------
 * User Space Combined Key Events
 */

#define	IN_SINGLE_PRESS					4
#define IN_PRESS_AND_HOLD				5
#define IN_DOUBLE_PRESS					6
#define IN_TRIPLE_PRESS					7
#define IN_QUAD_PRESS					8


/* stuff for the virtual simulator */
#define AT91SAM9G45_ID_SSC0		0
#define AT91SAM9G45_ID_SSC1		1

#define IN_SIM_KEY_UP_TIME		 200	// simulator is giving the KEY_UP event 200 ms after the KEY_DOWN event
#define IN_SIM_NEXT_PRESS_TIME 	 400	// simulator is giving the next KEY_DOWN event 350 ms after
										// the previous KEY_DOWN for a double/triple/quad press
#define IN_SIM_HOLD_UP_TIME		1200	// simulator gives KEY_UP 1 sec later for a PRESS_AND_HOLD event

#define	IN_PRESS_AND_HOLD_LIMIT	 980	// if the KEY_UP does not arrive within 980 ms 
										// after the KEY_DOWN then it is a PRESS_ANS_HOLD



#endif

