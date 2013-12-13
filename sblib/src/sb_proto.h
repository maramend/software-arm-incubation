/**
 * Low level EIB protocol handling
 */
#ifndef sb_proto_h
#define sb_proto_h

/**
 * Process the received telegram in sbRecvTelegram[]. Call this function when sbRecvTelegramLen > 0.
 * After this function, sbRecvTelegramLen shall/will be zero.
 */
extern void sb_process_tel();

/**
 * @return The number of send requests in the sending ring buffer sbSendRing.
 */
#define sb_send_ring_count() ((sbSendRingWrite - sbSendRingRead) & 7)


/**
 * The size of the ring buffer for send requests.
 */
#define SB_SEND_RING_SIZE 8

/**
 * Ring buffer for send requests.
 */
extern unsigned short sbSendRing[SB_SEND_RING_SIZE];

/**
 * Index in sbSendRing[] where the next write will occur.
 */
extern unsigned short sbSendRingWrite;

/**
 * Index in sbSendRing[] where the next read will occur.
 */
extern unsigned short sbSendRingRead;


#endif /*sb_proto_h*/
