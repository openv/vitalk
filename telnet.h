#ifndef PORT
#define PORT 83
#endif
#define MAX_DESCRIPTOR 20
#define TELNET_BUFFER_SIZE 80

extern short unsigned int vitalkport;

// Prototypes:
void telnet_init( void );
void telnet_task( void );

