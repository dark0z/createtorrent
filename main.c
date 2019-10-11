/**
 * BitTorrent spec requires dictionaries to be sorted!
 */

#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <openssl/sha.h>
#include <dirent.h>
#include <time.h>

/* to prevent buffer overflows */
#define MAX_RECURSION 29

void help_message();
int create_announce( const char*, const char*, const char*, const char*, const char*, int );
int create_from_file( const char*, FILE*, long long, int );
int create_from_directory( const char*, FILE*, int );
void write_name( const char*, FILE* );

const char* __version__ = "1.1.4";
int files = 0;
char* comment = NULL;
int inclusive = 0;
char* sha = NULL;
int shasize = 0;
int bytesin = 0;
char* buf = NULL;

int main( int argc, char** argv )
{
	int i;
	int optidx = 0;
	char* announce = NULL;
	char* src = NULL;
	char* outputfile = NULL;
	char* port = "6881";
	char* path = "/announce";
	int piecelen = 256 * 1024;

	while( 1 ) {
		static struct option options[] = {
			{ "announce", 1, 0, 'a' },  // announce
			{ "help", 0, 0, 'h' },
			{ "version", 0, 0, 'V' },
			{ "port", 1, 0, 'p' },
			{ "path", 1, 0, 'P' },
			{ "piecelength", 1, 0, 'l' },
			{ "comment", 1, 0, 'c' },
			{ "inclusive", 0, 0, 'i' },
			{ 0, 0, 0, 0 }
		};
		int c = getopt_long( argc, argv, "a:hiVp:P:l:c:", options, &optidx );
		if( c == -1 ) {
			break;
		}
		switch( c ) {
			case '?':
				return 1;
			case 'c':
				comment = optarg;
				break;
			case 'i':
				inclusive = 1;
				break;				
			case 'P':
				path = optarg;
				break;
			case 'a':
				announce = optarg;
				break;
			case 'p':
				port = optarg;
				break;
			case 'h':
				help_message();
				return 0;
			case 'V':
				printf( "createtorrent %s (Daniel Etzold <detzold@gmx.de>)\n", __version__ );
				return 0;
			case 'l':
				piecelen = atoi( optarg );
				break;
		}
	}

	if( piecelen <= 0 ) {
		fprintf( stderr, "piece length has to be > 0.\n" );
		return 1;
	}

	for( i = optind; i < argc; ++i ) {
		if( ! src ) {
			src = argv[ i ];
		} else {
			outputfile = argv[ i ];
		}
	}
	if( announce && src && outputfile && port && path ) {
		return create_announce( announce, src, outputfile, port, path, piecelen );
	}

	fprintf( stderr, "Invalid arguments. Use -h for help.\n" );
	return 1;
}

int create_announce( const char* announce, const char* src, const char* output, const char* port, const char* path, int piecelen )
{
	struct stat s;
	int ret = 0;
	int p = atoi( port );

	// "announce", "comment", "creation date", "info"
	
	if( p ) {
		FILE* f = fopen( output, "w" );
		if( f ) {
			// Open main dictionary and write "announce"
			fprintf( f, "d8:announce%d:%s:%s%s", 
				strlen( announce ) + strlen( path ) 
				+ strlen( port ) + 1, announce, port, path );

			// "comment"
			if( comment ) {
				fprintf( f, "7:comment%d:%s", 
					strlen( comment ), comment );
			}

			// "creation date"
			fprintf( f, "13:creation datei%de", (unsigned) time( NULL ) );

			// "info" dictionary
			fputs( "4:infod", f );

			// write file info
			if( ! stat( src, &s ) ) {
				if( S_ISDIR( s.st_mode ) ) {
					ret = create_from_directory( src, f, piecelen );
				} else if( S_ISREG( s.st_mode ) ) {
					ret = create_from_file( src, f, s.st_size, piecelen );
				} else {
					fprintf( stderr, "not a file or directory: %s\n", src );
					ret = 1;
				}
			} else {
				fprintf( stderr, "could not access %s: %m\n", src );
				ret = 1;
			}

			// Close info and main dictionary
			fputs( "ee", f );
			fclose( f );
			if( ret ) {
				unlink( output );
			}
		} else {
			fprintf( stderr, "could not open destination file %s\n", output );
			ret = 1;
		}
	} else {
		fprintf( stderr, "invalid port number\n" );
		ret = 1;
	}
	return ret;
}

void write_name( const char* name, FILE* f )
{
	const char* p = name + strlen( name ) - 1;
	const char* l;
	char* c;
	while( *p == '/' && p != name ) --p;
	l = p;
	while( p != name && *( p - 1 ) != '/' ) --p;
	c = strdup( p );
	c[ l - p + 1 ] = 0;
	fprintf( f, "4:name%d:%s", l - p + 1, c );
	free( c );
}

int create_from_file( const char* src, FILE* f, long long fsize, int piecelen )
{
	char* sha;
	char* buf;
	int r, shalen;
	FILE* fs = fopen( src, "rb" );

	// "length", "name", "piece length", "pieces"

	fprintf( f, "6:lengthi%llde", fsize );
	write_name( src, f );
	fprintf( f, "12:piece lengthi%de", piecelen );

	shalen = fsize / piecelen;
	if( fsize % piecelen )
		shalen++;
	fprintf( f, "6:pieces%d:", shalen * 20 );

	if( fs ) {
		sha = (char*) malloc( sizeof(char) * SHA_DIGEST_LENGTH );
		buf = (char*) malloc( piecelen );
		printf( "computing sha1... " );
		fflush( stdout );
		while( ( r = fread( buf, 1, piecelen, fs ) ) ) {
			SHA1( buf, r, sha );
			fwrite( sha, 1, 20, f );
		}
		free( buf );
		free( sha );
		fclose( fs );
		printf( "done\n" );
	} else {
		fprintf( stderr, "could not open file %s\n", src );
		return 1;
	}
	return 0;
}

/**
 * ByteEncode file name:
 *   name = "CD1/Sample/myfile.avi"
 *       -> "pathl3:CD16:Sample10:myfile.avi"
 **/
void format_path( char *in, char *out )
{
	char *s;
	char t[1024];
		
	memcpy(out,"pathl",5);
	s = strtok(in, "/");
	while (s) {
		snprintf(t, sizeof(t), "%i:%s", strlen(s), s);
		strcat(out, t);
		s = strtok(NULL, "/");			
	}
}

int add_file(FILE *torrent, long long fsize, char *fname, int piecelen, const char *strip)
{
	FILE* fs = fopen( fname, "rb" );

	// "length", "path"
	
	printf( "adding %s\n", fname );
	if( fs ) {
		int r;
		char fname_t[8092];
		memset(fname_t, '\0', sizeof(fname_t));
		format_path( fname + strlen(strip) + 1, fname_t );
		fprintf( torrent, "d6:lengthi%llde4:%see", fsize, fname_t );
		while( ( r = fread( buf + bytesin, 1, piecelen - bytesin, fs ) ) ) {
			bytesin += r;
			if( bytesin == piecelen ) {
				sha = (char*) realloc( sha, shasize + 20 );
				SHA1( buf, bytesin, sha + shasize );
				shasize += 20;
				bytesin = 0;
			}
		}
		fclose( fs );
		return 1;
	} else {
		fprintf( stderr, "could not open file %s\n", fname );
		return 0;
	}	
}

/* recursively scan directories & add files */
int process_directory(DIR *d, FILE *torrent,char *p,int level, int piecelen, const char* strip)
{
	char path[ 8092 ];
	struct dirent* n;
	DIR* dir;

	while( ( n = readdir( d ) ) ) {
		struct stat s;
		char fbuf[ 8092 ];
		snprintf( fbuf, sizeof(fbuf), "%s/%s", p, n->d_name );
		if( ! stat( fbuf, &s ) ) {
			if( S_ISREG( s.st_mode ) ) {
				if( n->d_name[0] == '.' && !inclusive )
					continue;
				if( add_file(torrent,(unsigned int) s.st_size, fbuf ,piecelen, strip) )
					files++;		
			} else if( S_ISDIR (s.st_mode) ){
				if( strcmp(n->d_name,".") && strcmp(n->d_name,"..") ) {
					if(++level > MAX_RECURSION)
						return 1;
					snprintf( path, sizeof(path), "%s/%s",p,n->d_name );
					dir = opendir( path );
					if (!dir) {
						printf("skipping directory %s (%m)\n", path);
					} else {
						process_directory(dir, torrent, path, level, piecelen, strip);
					}
				}
			} else {
				printf( "ignoring %s (no directory or regular file)\n", fbuf );
			}
		} else {
			fprintf( stderr, "could not stat file %s\n", fbuf );
		}
	}
	return 0;		
}
	
int create_from_directory( const char* src, FILE* f, int piecelen )
{
	int ret = 0;
	DIR* dir = opendir( src );

	// "files", "name", "piece length", "pieces"
	
	if( dir ) {
		char src_p[1024];
		buf = (char*) malloc( piecelen );

		// start of the "files" list
		fputs( "5:filesl", f );
		snprintf(src_p, sizeof(src_p), "%s", src);
		process_directory(dir, f, src_p, 0, piecelen, src);
		fputs( "e", f );  // mark end of "files" list

		// "name"
		write_name( src, f );

		// "piece length"
		fprintf( f, "12:piece lengthi%de", piecelen );

		// "pieces"
		if( shasize || bytesin ) {		
			// first e comes from file list
			fprintf( f, "6:pieces%d:", shasize + ( ( bytesin > 0 ) ? 20 : 0 ) );
			if( sha ) {
				fwrite( sha, 1, shasize, f );
			}
			if( bytesin ) {
				char sha[ 20 ];
				SHA1( buf, bytesin, sha );
				fwrite( sha, 1, 20, f );
			}
		}
		if( sha ) {
			free( sha );
		}
		free( buf );
		if( ! files ) {
			fprintf( stderr, "warning: no files found in directory\n" );
		}
		closedir( dir );
	} else {
		fprintf( stderr, "could not read directory %s\n", src );
		ret = 1;
	}
	return ret;
}

void help_message()
{
	printf( "Usage: createtorrent [OPTIONS] -a announce <input file or directory> <output torrent>\n\n"
		"options:\n"
		"--announce    -a  <announceurl> : announce url\n"
		"--port        -p  <port>        : sets the port, default: 6881\n"
		"--path        -P  <path>        : sets the path on the server, default: /announce\n"
		"--piecelength -l  <piecelen>    : sets the piece length in bytes, default: 262144\n"
		"--comment     -c  <comment>     : adds an optional comment to the torrent file\n"
		"--inclusive   -i                : include hidden *nix files (starting with '.')\n"
		"--version     -V                : version of createtorrent\n"
		"--help        -h                : this help screen\n"
	);
}

