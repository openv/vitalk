#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "vito_parameter.h"
#include "vito_io.h"
#include "telnet.h"


short unsigned int vitalkport;

extern fd_set master_fds; /* file descriptor list for select() */
extern fd_set read_fds;   /* ergebnis des select() Aufrufs */

static int fd_listener; /* listening socket descriptor */
static struct sockaddr_in serveraddr; /* server address */
static struct sockaddr_in clientaddr; /* client address */

// Per telnet aufrufbare Befehle:
const char *commands[] =
{  "help",              // 0
   "h",                 // 1
   "get",               // 2
   "g",                 // 3
   "set",               // 4
   "s",                 // 5
   "list",              // 6
   "gc",                // 7
   "gvu",               // 8
   "frame_debug",       // 9
   "\0"
};

// Hilfstext
static void print_help( int fd )
{
  dprintf(fd,
	  "Short Help Text:\n"
	  "  h, help                 - This Help Text\n"
	  "  frame_debug <on/off>    - Set state of Frame Debugging\n"
	  "  list [class]            - Show parameter List\n"
	  "  g, get <p_name>         - Query Parameter\n"
	  "  gvu <p_name>            - Get Value of Parameter with Unit\n"
	  "  s, set <p_name> <value> - Set Parameter\n"
	  "  gc <class>              - Query a class of Parameters\n"
	  "     Parameterklassen:\n"
	  "       P_ALLE       0\n"
	  "       P_ERRORS     1\n"
	  "       P_ALLGEMEIN  2\n"
	  "       P_KESSEL     3\n"
	  "       P_WARMWASSER 4\n"
	  "       P_HEIZKREIS  5\n"
	  "       P_BRENNER    6\n"
	  "       P_HYDRAULIK  7\n"
	  "\n"
    );
}

// Parameterklasse abfragen:
static void get_class( int fd, int p_class )
{
  int i=0;
  while( parameter_liste[i].p_name )
    {
      if ( ( p_class == 0 && parameter_liste[i].p_class > 1 ) || // ganze liste ohne Errors
	   p_class == parameter_liste[i].p_class )               // spezifizierte Klasse
	dprintf(fd, "%02u: %20s: %s %s\n",
		parameter_liste[i].p_class,
		parameter_liste[i].p_name,
		get_v(parameter_liste[i].p_name),
		get_u(parameter_liste[i].p_name)
	       );
      i++;
    }
  dprintf(fd, "\n");
}
  
// Liste aller Parameter
static void print_listall( int fd, int p_class )
{
  int i=0;
  while( parameter_liste[i].p_name )
    {
      if ( p_class == 0 || p_class == parameter_liste[i].p_class )
	dprintf(fd, "%02u: %20s: %s\n",
		parameter_liste[i].p_class,
		parameter_liste[i].p_name,
		parameter_liste[i].p_description
	       );
      i++;
    }
  dprintf(fd, "\n");
}

// Init Telnet Socket:
void telnet_init( void )
{
  FD_ZERO(&master_fds);
  
  /* create socket */
  if((fd_listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1)
    {
      fprintf( stderr, "Error creating Telnet Socket: %s\n", strerror(errno));
      exit(1);
    }

  // Set REUSEADDR option
  // Sonst kann man nach einem Programmabbruch laengere Zeit nicht neu starten:
  const int optVal = 1;
  if ( setsockopt(fd_listener, SOL_SOCKET, SO_REUSEADDR, (void*) &optVal, sizeof(int)) == -1 )
    {
      fprintf( stderr, "Error setting Telnet Socket Options: %s\n", strerror(errno));
      exit(1);
    }
  
  /* bind */
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = INADDR_ANY;
  serveraddr.sin_port = htons(vitalkport);
  memset(&(serveraddr.sin_zero), '\0', sizeof(serveraddr.sin_zero) );
  if(bind(fd_listener, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1)
    {
      fprintf( stderr, "Error bind Telnet Socket: %s\n", strerror(errno));
      exit(1);
    }
  
  /* listen */
  if(listen(fd_listener, 10) == -1) // "10" = listen backlog
    {
      fprintf( stderr, "Error listen on Telnet Socket: %s\n", strerror(errno));
      exit(1);
    }
  else
    {
      fprintf( stderr, "Now listening to telnet Port %d\n", PORT );
    }
  
  /* add the listener to the master set */
  FD_SET(fd_listener, &master_fds);
}

// Folgende Funktion wird periodisch nach der select() Rueckkehr aufgerufen.
// Es muss die von select() zurueckgegebene Descriptormenge uebergeben werden:
void telnet_task( void )
{
  int i;
  int fd_new;
  static char buffers[MAX_DESCRIPTOR + 1][TELNET_BUFFER_SIZE];
  static int buf_ptr[MAX_DESCRIPTOR + 1];
  int result;
  int cnum;
  
  // Traverse over all possible Filedescriptors:
  for(i = 0; i <= MAX_DESCRIPTOR; i++)
    {
      if( FD_ISSET(i, &read_fds) )
	{ // Neue Verbindung?
	  if( i == fd_listener )
	    { // Neue Verbindung!
	      socklen_t addrlen = sizeof(clientaddr);
	      if( ( fd_new = accept(fd_listener, (struct sockaddr *)&clientaddr, &addrlen)) == -1)
		  fprintf(stderr, "Error accepting Connection!\n");
	      else
		{
		  if ( fd_new > MAX_DESCRIPTOR )
		    {
		      fprintf(stderr, "Max Number of Telnet Connections reached!\n");
		      close( fd_new );
		    }
		  else
		    {
		      fprintf(stdout, "New connection from %s on socket %d\n",
			      inet_ntoa(clientaddr.sin_addr), fd_new);
		      FD_SET(fd_new, &master_fds); /* add to master set */
		      buffers[i][0]='\0';
		      buf_ptr[i] = 0;
		      dprintf( fd_new, "Welcome at vitalk, the Vitodens telnet Interface. (%u)\n", fd_new);
		    }
		}
	    }
	  else
	    { // Daten von einem Client:
	      result = recv(i, &buffers[i][buf_ptr[i]], 1, MSG_DONTWAIT); // Nicht effektiv, aber einfacher
	      if ( result <= 0 )
		{
		  if ( result < 0 )
		    fprintf( stderr, "recv() error: %s\n", strerror(errno));
		  // Verbindung wurde beendet:
		  fprintf(stdout, "Socket %d hung up.\n", i);
		  close(i);
		  FD_CLR(i, &master_fds);
		}
	      else
		{ // Echte Daten empfangen:
		  buf_ptr[i]++;
		  buffers[i][buf_ptr[i]]='\0';
		  if ( ( strchr(buffers[i], '\n') ) || // Newline im Puffer?
		       ( buf_ptr[i] > TELNET_BUFFER_SIZE - 4 ) ) // Oder Puffer "ziemlich" voll?
		    { // Dann werten wir das aus:
		      char command[24] = "";
		      char value1[24] = "";
		      char value2[24] = "";

		      sscanf( buffers[i], "%20s %20s %20s %*s\n", command, value1, value2 );
		      
		      buf_ptr[i] = 0; // Fuer die naechste Pufferfuellung
			      
		      // Suche nach bekanntem Befehl:
		      for ( cnum=0; commands[cnum][0]; cnum++ ) 
			{
			  if ( strcmp( commands[cnum], command ) == 0 )
			    switch( cnum )
			      {
			      case 0:
			      case 1:
				print_help(i);
				break;
			      case 2:
			      case 3:
				dprintf( i, "%s\n", get_v(value1) );
				break;
			      case 4:
			      case 5:
				dprintf( i, "%s\n", set_v(value1, value2) );
				break;
			      case 6:
				print_listall( i, atoi(value1) );
				break;
			      case 7:
				get_class( i, atoi(value1) );
				break;
			      case 8:
				dprintf( i, "%s %s\n", get_v(value1), get_u(value1) );
				break;
			      case 9:
				if ( strcmp( value1, "on" ) == 0 )
				  frame_debug = 1;
				else
				  frame_debug = 0;
				break;
			      }
			}
		    }
		}
	    }
	}
    }
}
