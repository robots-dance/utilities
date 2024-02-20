#include <stdio.h>
#include <windows.h>

#define BAD_PARAMS 1
#define CANT_OPEN_INPUT_FILE 2
#define CANT_GET_TIME 3
#define CANT_SET_TIME 4

HANDLE OpenInputFile( LPCWSTR, DWORD );

int wmain( int argc, wchar_t **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "Usage: ft-rollback.exe <file-path>\n" );
		return BAD_PARAMS;
	}
	LPCWSTR filePath = argv[ 1 ];
	
	HANDLE hFile = OpenInputFile( filePath, GENERIC_READ );
	if ( NULL == hFile )
	{
		fprintf( stderr, "can't open an input file\n" );
		return CANT_OPEN_INPUT_FILE;
	}
	
	FILETIME creatTime;
	FILETIME lastAccessTime;
	FILETIME lastWriteTime;
	
	if ( !GetFileTime( hFile, &creatTime,
		&lastAccessTime, &lastWriteTime ) )
	{
		CloseHandle( hFile );
		fprintf( stderr, "can't get times: %d\n", GetLastError() );
		return CANT_GET_TIME;
	}
	CloseHandle( hFile );
	
	printf( "please enter any key after the file changing..." );
	fgetc( stdin );
	
	hFile = OpenInputFile( filePath, GENERIC_WRITE );
	if ( NULL == hFile )
	{
		fprintf( stderr, "can't open the input file\n" );
		return CANT_OPEN_INPUT_FILE;
	}
	if ( !SetFileTime( hFile, &creatTime,
		&lastAccessTime, &lastWriteTime ) )
	{
		CloseHandle( hFile );
		fprintf( stderr, "can't set times: %d\n", GetLastError() );
		return CANT_SET_TIME;
	}
	CloseHandle( hFile);
	
	return 0;
}

HANDLE OpenInputFile( LPCWSTR inpFilePath, DWORD access )
{
	HANDLE hFile = CreateFileW( inpFilePath,
		access,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL );
	return INVALID_HANDLE_VALUE == hFile ? NULL : hFile;
}
