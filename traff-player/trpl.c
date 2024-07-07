#include <winsock2.h>
#include <stdio.h>

#define MAX_HEADER_SIZE 8

#define OUT_FILE_NAME "session.bin"

// error messages
#define FILE_READ_ERR_MESS "cannot read file!\n"
#define SEND_DATA_ERR_MESS "cannot send data!\n"
#define READ_DATA_ERR_MESS "cannot read data!\n"
#define USAGE_MESS "Usage: trpl.exe <dump-file-path> <host> <port>\n"

// error codes
#define BAD_ARGS_COUNT 1
#define CANT_RESOLVE_ADDR 2
#define CANT_CONNECT_TO_SERVER 3
#define CANT_OPEN_IN_FILE 4
#define CANT_OPEN_OUT_FILE 5
#define END_OF_FILE 6
#define BAD_READ_FILE_OP 7
#define BAD_HEADER 8
#define CANT_SEND_DATA 10
#define CANT_READ_DATA 11
#define BAD_TCP_PORT 12

const char kSendHeader[] = { '[', 's', 'e', 'n', 'd', ']' };
const char kRecvHeader[] = { '[', 'r', 'e', 'c', 'v', ']' };

typedef unsigned char uchar;
typedef unsigned int uint32_t;

typedef enum {
	e_RecvHeader = 0,
	e_SendHeader,
	e_BadHeader
} HeaderType;

// files functions
HeaderType read_header( FILE *file );
void skip_header( FILE *file, HeaderType );
int read_int( FILE * );
void write_int( FILE *, int );
// --

// network functions
uint32_t resolv( char *host );
int tcp_send( SOCKET sd, uchar *inpBuff, int size );
int tcp_recv( SOCKET sd, uchar *outBuff, int buffSize );
// --

int main( int argc, char **argv )
{
	// check input parameters
	if ( argc < 4 )
	{
		fprintf( stderr, USAGE_MESS );
		return BAD_ARGS_COUNT;
	}
	// --
	
	// initialization
	WSADATA wsadata;
	WSAStartup( MAKEWORD( 1, 0 ), &wsadata );
	struct sockaddr_in addr;
	
	int port = atoi( argv[ 3 ] );
	if ( port < 0 || port > 65535 )
	{
		fprintf( stderr, "incorrect input port\n" );
		return BAD_TCP_PORT;
	}
	
	char *host = argv[ 2 ];
	if ( ( addr.sin_addr.s_addr = resolv( host ) ) == INADDR_NONE )
	{
		fprintf( stderr, "cannot resolve address\n" );
		return CANT_RESOLVE_ADDR;
	}
	
	addr.sin_port = htons( port );
	addr.sin_family = AF_INET;
	// --
	
	// connect to the server
	SOCKET sd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
	if( connect( sd, ( struct sockaddr * )&addr,
		sizeof( struct sockaddr_in ) ) < 0 )
	{
		fprintf( stderr, "cannot connect to server\n" );
		return CANT_CONNECT_TO_SERVER;
	}
	// --
	
	// prepare input and output files 
	const char *dumpFilePath = argv[ 1 ];
	FILE *inpFile = fopen( dumpFilePath, "rb" );
	if ( NULL == inpFile )
	{
		fprintf( stderr, "cannot open input file\n" );
		return CANT_OPEN_IN_FILE;
	}
	FILE *outFile = fopen( OUT_FILE_NAME, "wb" );
	if ( NULL == outFile )
	{
		fclose( inpFile );
		printf( "cannot open output file\n" );
		return CANT_OPEN_OUT_FILE;
	}
	// --
	
	// start a player process
	BOOL end = FALSE;
	int error = 0;
	uchar *sendBuff = NULL;
	uchar *recvBuff = NULL;
	while ( !end && 0 == error )
	{
		free( sendBuff ); sendBuff = NULL;
		free( recvBuff ); recvBuff = NULL;
		
		HeaderType ht = read_header( inpFile );
		if ( e_SendHeader ==ht )
		{
			int toSendSize = read_int( inpFile );
			error = feof( inpFile ) != 0 ? END_OF_FILE : 0;
			if ( error > 0 ) {
				break;
			}
			else {
				sendBuff = ( uchar* )malloc( toSendSize );
			}
			
			printf( "bytes to send %d\n", toSendSize );
			error = ( fread( sendBuff, sizeof( char ), toSendSize,
				inpFile ) != toSendSize ) ? BAD_READ_FILE_OP : 0;
			if ( error > 0 )
			{
				break;
			}
			end = feof( inpFile ) != 0; // no needed
			
			// send a request to the server
			if ( tcp_send( sd, sendBuff, toSendSize ) != toSendSize )
			{
				fprintf( stderr, "cannot send data to server\n" );
				error = CANT_SEND_DATA;
			}
			else
			{
				fwrite( kSendHeader, sizeof( char ), sizeof( kSendHeader ),
					outFile );
				write_int( outFile, toSendSize );
				fwrite( sendBuff, sizeof( char ), toSendSize, outFile );
			}
			// --
		}
		else if ( e_RecvHeader == ht )
		{
			// skip a read section
			int toRecvSize = read_int( inpFile );
			fseek( inpFile, toRecvSize, SEEK_CUR );
			end = feof( inpFile ) != 0; // no needed
			// --
			
			// get response from server
			recvBuff = ( uchar* )malloc( toRecvSize );
			int readedCount = tcp_recv( sd, recvBuff, toRecvSize );
			printf( "bytes to recv o: %d n: %d\n", toRecvSize, readedCount );
			if ( readedCount <= 0 )
			{
				fprintf( stderr, "cannot read data from server\n" );
				error = CANT_READ_DATA;
			}
			else
			{
				fwrite( kRecvHeader, sizeof( char ), sizeof( kRecvHeader ),
					outFile );
				write_int( outFile, readedCount );
				fwrite( recvBuff, sizeof( char ), readedCount, outFile );
			}
			// --
		}
		else {
			error = BAD_HEADER;
		}
		end = feof( inpFile ) != 0;
	}
	// --
	
	// check errors
	if ( END_OF_FILE == error )
	{
		fprintf( stderr, "end of file\n" );
	}
	else if ( BAD_READ_FILE_OP == error )
	{
		fprintf( stderr, "bad read file operation\n" );
	}
	else if ( BAD_HEADER == error )
	{
		fprintf( stderr, "bad header\n" );
	}
	else if ( error != 0 )
	{
		fprintf( stderr, "unknown error: %d\n", error );
	}
	// --
	
	shutdown( sd, SD_BOTH );
	closesocket( sd );
	
	fclose( outFile );
	fclose( inpFile );
	
	return error;
}

// files functions
HeaderType read_header( FILE *file )
{
	char buff[ MAX_HEADER_SIZE ] = { 0 };
	fread( buff, sizeof( char ), MAX_HEADER_SIZE, file );
	fseek( file, -1 * MAX_HEADER_SIZE, SEEK_CUR );
	HeaderType result = e_BadHeader;
	if ( !strncmp( buff, kSendHeader, sizeof( kSendHeader ) ) )
	{
		result = e_SendHeader;
		fseek( file, sizeof( kSendHeader ), SEEK_CUR );
	}
	else if ( !strncmp( buff, kRecvHeader, sizeof( kRecvHeader ) ) )
	{
		result = e_RecvHeader;
		fseek( file, sizeof( kRecvHeader ), SEEK_CUR );
	}
	return result;
}

void skip_header( FILE *file, HeaderType ht )
{
	int bytesSkipped = e_RecvHeader == ht ?
		sizeof( kRecvHeader ) : sizeof( kSendHeader );
	fseek( file, bytesSkipped, SEEK_CUR );
}

int read_int( FILE *file )
{
	int result = 0;
	uchar buff[ 4 ];
	fread( buff, sizeof( uchar ), sizeof( buff ), file );
	for ( int i = 0; i < sizeof( buff ); ++i )
	{
		int part = buff[ i ];
		for ( int j = 0; j < i; ++j ) {
			part <<= 8;
		}
		result |= part;
	}
	return result;
}

void write_int( FILE *file, int value )
{
	char buff[ 4 ];
	int number = value;
	for ( int i = 0; i < sizeof( buff ); ++i )
	{
		buff[ i ] = ( char )number;
		number >>= 8;
	}
	fwrite( buff, sizeof( char ), sizeof( buff ), file );
}
// --

// network functions
uint32_t resolv( char *host )
{
	struct hostent *hp;
	uint32_t hostIp;
	
	hostIp = inet_addr( host );
	if( INADDR_NONE == hostIp )
	{
		hp = gethostbyname( host );
		if( hp ) {
			hostIp = *( uint32_t* )hp->h_addr;
		}
	}
	return hostIp;
}

int tcp_send( SOCKET sd, uchar *inpBuff, int size )
{
	int result = 0;
	int curSize = size;
	while ( curSize > 0 )
	{
		int sended = send( sd, ( char* )inpBuff, curSize, 0 );
		if ( sended <= 0 )
		{
			result = -1;
			break;
		}
		else
		{
			curSize -= sended;
			inpBuff += sended;
			result += sended;
		}
	}
	return result;
}

int tcp_recv( SOCKET sd, uchar *outBuff, int buffSize )
{
	return recv( sd, ( char* )outBuff, buffSize, 0 );
	// because we use very large buffer
}
// --

