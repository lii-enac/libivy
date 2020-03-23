#ifndef	PARAM_H
#define PARAM_H



/*
  default port number and broadcast adress for any agent using
  ivy bus
 */
#define IVY_DEFAULT_BUS 2010
#define IVY_DEFAULT_DOMAIN 127.255.255.255

/*
  These constants are arbitrary and can be lowered to adapt the lib
  to tiny environments, or raised to adapt lib to optimise lib in
  case of very big transaction number
 */


/*
  max message length
 */
#define IVY_BUFFER_SIZE 4096

/*
  max number of captured field by a regexp
 */
#define IVY_MAX_MSG_FIELDS 200

/*
  max number of captured field by a regexp
 */
#define IVY_MAX_REGEXP 4096


/*
  if congestion occurs, fifo size will be raised by bock of this size.
  the biggest it is, the least realloc/recopy occurs, and this realloc/recopy  
  could be very slow, so if memory footprint is not a problem, 
  keep this limit high
 */
#define IVY_FIFO_ALLOC_SIZE 262144


/*
  Beginning with version 3.11, message send is non blocking, 
  if receiver is congestionned, messages are accumulated in a local
  fifo buffer. This is the maximum size for the fifo, after that, messages
  will not be sent
 */
#define IVY_FIFO_MAX_ALLOC_SIZE  (32 * 1048576)

/*
  maximum number of arguments which can be catched by a regexp
 */
#define MAX_MATCHING_ARGS 120


/* TCP_NODELAY is for a specific purpose; to disable the Nagle buffering
   algorithm. It should only be set for applications that send frequent
   small bursts of information without getting an immediate response,
   where timely delivery of data is required (the canonical example is
   mouse movements).

   Since Ivy is most of the time used to send events, we will priviligiate
   lag over throughtput, so  _TCP_NO_DELAY_ACTIVATED is set to 1
*/
extern const int  TCP_NO_DELAY_ACTIVATED;



#endif // PARAM_H
