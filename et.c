//misc
//
// et (tinyeditor) - the extremely tiny (8k 32bit linux, 20k OSX 64bit) editor,
//   statically compiled with minilib
//
//	compile: make
//	install: make install
//
//   Based on work done by Terry Loveall (2005) and Anthony Howe (1991)
//
// Added Cursor keys and function keys (extended keycodes), debug, "vi" mode (enable with define TM)
//   ported to x64 OSX, ported to minilib
//
//   Some minor changes
//
// Michael (Misc) Myer misc.myer@zolo.com
//   2013-2019
//
// edit: vi-modus. aes encryption
//
// am besten: escape sequences als union { integer[2],char[8] } speichern.
// zum Vergleichen scheinen die letzten 4 (von 5) Werten der Sequenz zu genügen,
// also int[1].
// besser: union { struct k { char empty[3], char seq[5] }; struct i { int e, int key } };
// (translate above)
//
// TODO:
// load file (f3)
// save as 
// Search and replace
// possibly: malloc the buf / use swap file
// keycodetable: ab [32] von vorne anfangen lassen. -> doppelte tastenbelegung / oder: mehrdimensionales array
// ( z.B. ^H für Backspace ) möglich: beim Aufruf: [i & 31] ... 
// -1 für unbelegte tasten.

/* ue is a libc only text editor.
	 compile with:
	 gcc -g -O2 -fomit-frame-pointer -pipe -Wall -o ue ue.c;sstrip ue;ls -al ue
	 Public Domain, (C) 2005 Terry Loveall,
	 THIS PROGRAM COMES WITH ABSOLUTELY NO WARRANTY OR BINARIES. COMPILE AND USE
	 AT YOUR OWN RISK.
usage: ue <filename> # <filename> NOT optional
*/

#define VERSION "1.50"

//#define dodebug
//#define TM  // viish toggle-mode. Switch with # between edit / move

#ifdef MLIB
#include "minilib.h"
#else
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#endif

#ifndef POINTER
#ifndef X64
#define POINTER int
#else
#define POINTER long int
#endif
#endif

// max len of file
#define BUF 1024*1024
#define MODE 0666
#define TABSZ 4
#define TABM TABSZ-1

#define MAXCOLUMNS 512
int termcolumns=80;
int termlines=24;

const char helpt[] =
"\net ver. 155\n"
"The tiny (8.8k Linux 64bit) editor.\n\n"
"\n"
"2014-2019 Michael (Misc) Myer\n"
"Based on work done by Terry Loveall (2005) and Anthony Howe (1991)\n"
"\n"
"Keys:\n"
"\n"
"Alt+Left: Word left     Alt+Right: Word right\n"
"Shift+Ü: Top of file    Shift+Ä: End of file\n"
"Alt+D: Delete line      Alt+R: Redraw screen\n"
"Alt+U: Undo\n"
"\n"
"Alt+H: This Help           Alt+W: Save\n"
"Alt+F: Find              \n"
"Alt+N: Find next           Alt+P: Find previous\n"
"Alt+G: Goto line           Alt+Q: Quit\n"
"\n"
"		Push any key to continue\n\n\n\n";

int line_count = 1;
int done;
int row, col;
char clearscreen;
#define PROMPTMAX 64
// 64 bytes should be enough for everyone ... misc
char ln[PROMPTMAX];       // goto line string, used by gotoln()
char ss[PROMPTMAX];       // search string, used by find()

char ubuf[BUF];         // undo buffer
char buf[BUF];          // the edit buffer
char *ebuf = buf + BUF; // end of edit buffer
char *etxt = buf;       // end of edit text
char *curp = buf;       // current position in edit buffer
char *pagestart, *pageend;     // start and end of displayed edit text
char *filename;         // file being edited
//char ch;                // most recent command/edit character
struct termios termios, orig;

//misc v

int mode = 0; // 0=move, 1=edit


#ifdef dodebug
#define debug(...) fprintf(dbg, __VA_ARGS__)
int dbg = 0;
#else
#define debug(...)
#endif

union keybuf {
		struct { char unuesedc[2], key, keyseq[5]; };
		struct { int unusedi, intkeyseq; };
};
//misc ^

// undo record struct
typedef struct {
		char ch;
		POINTER pos;
} U_REC;
U_REC* undop = (U_REC*)&ubuf;

typedef struct {
		int X;
		int Y;
} COORD;
COORD xy;

#define writen(str,n) write(STDOUT_FILENO,str,n)
#define writestr(str) writen(str,sizeof(str)-1)
#define write1(str) writen(str,strlen(str))
#define getch() (char)getkey()


void GetSetTerm(int set){
		struct termios *termiop = &orig;
		if(!set) {
				tcgetattr(0,&orig);
				termios = orig;
				termios.c_lflag    &= (~ICANON & ~ECHO );//& ~ISIG);
				termios.c_iflag    &= (~IXON & ~ICRNL);
				termios.c_cc[VMIN]  = 1;
				termios.c_cc[VTIME] = 0;
				termiop = &termios;
		} 
		tcsetattr(0, TCSANOW, termiop); 
} 


void highlight(int hl){ 
		hl ? writestr("\033[44m\033[2m\033[37m") : writestr("\033[0m");   // black char on white background
}

void gotoxy(int x, int y){
		union { char str[12]; int i[2];} u;
		u.i[2] = 0;
		sprintf(u.str,"\033[%d;%dH\0",y,x);
		write1(u.str);
		xy.Y=y; xy.X=x;
}

//misc v
//

void switchmode(){
		if ( mode ){
				mode = 0;
		} else {
				mode = 1;
		}
}

void getwinsize(){
		struct winsize w;
		ioctl(fileno(stdout), TIOCGWINSZ, &w);
		debug ("lines %d\n", w.ws_row);
		termlines = w.ws_row-2; // last line unused, 
		// the newline would need to be filtered
		debug ("columns %d\n", w.ws_col);
		termcolumns = w.ws_col;
		//termcolumns = 80;
		//termlines=30;
		return; 
}



// return a positive value, if a key has been hit / a sequence has been 
// read only partially.
// 0 otherwise
int keyhit(){
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 500;

		fd_set set;
		FD_ZERO(&set);
		FD_SET(fileno(stdin), &set);
		return (select(fileno(stdin) + 1, &set, NULL, NULL, &tv));
}

#ifndef TM

int getkey(){
		union _kbuf { 
				struct { int u1; char key, u2[3]; };
				struct { char u[3], seqstart; int keycode; };
				char seq[8];
		} kb;

		kb.keycode = 0;
		read(0, (POINTER*)&kb.key, 1);

		if ((kb.key == 0x1b) && (keyhit())) { 
				kb.keycode = 0xFF000000;
#ifdef OSX
				read(0, (void *) &kb.seqstart+1, 5);
#else
				read(0, (void *) &kb.seqstart, 5);
#endif
		}
		//int a;
		debug("seq: 27 " );
		//for ( a=4;a<=7;a++ )
		//		debug( "%d ", kb.seq[a] );
		debug("\n");
		debug("keycode: 0x%x\n",kb.keycode);
		debug("c: %u\n", kb.key);
		return (kb.keycode);
}

#else

char getkey(){
		char key;
		char seq[8];

		do {
		read(0, &key, 1);
		if ( key == 0x1b && keyhit() ){
				read(0, (void *) &seq, 5);
				key=0;
		}
		} while (  ((key<0xa) || (key>0x7f))  );

		return(key);
}

#endif
//misc ^

void put1(char c){
		writen(&c,1);
		xy.X++;
}

#pragma GCC push_options
#pragma GCC optimize("O0")

void emitch(char c){
		if(c == '\t') 
				do { put1(' '); } while((xy.X-1) & (TABM));
		else 
				put1(c);
		if(c == '\n') {
				xy.X=1; 
				xy.Y++;
		}
}

#pragma GCC pop_options

void clrtoeol(){
		int ox = xy.X, oy = xy.Y;
		while((xy.X-1) < termcolumns) put1(' ');
		gotoxy(ox,oy);
}

char *prevline(char *p){
		while (buf < --p && *p != '\n') ;
		return (buf <= p ? ++p : buf);
}

char *nextline(char *p){
		while (p < etxt && *p++ != '\n') ;
		return (p);
}

char *adjust(char *p, int column){
		int i = 0;
		while (p < etxt && *p != '\n' && i < column) {
				i += *p++ == '\t' ? TABSZ-(i & (TABM)) : 1;
		}
		return (p);
}

int scmp(char* t, char* s, int c){
		int i;
		for( i=0; i < c; i++) if(s[i]-t[i]) return (s[i]-t[i]);
		return (0);
}

void cmove(char *src, char *dest, int cnt){
		if(src > dest) while(cnt--) *dest++ = *src++;
		if(src < dest) {
				src += cnt;
				dest += cnt;
				while(cnt--) *--dest = *--src;
		}
		etxt += dest-src;
}

void left(){
		if(buf < curp) --curp;
}

void down(){
		if ( curp == etxt )
				return;
		debug("%x  %x\n",curp, etxt );
		curp = adjust(nextline(curp), col);
}

void up(){
		debug("%x  %x\n",curp, buf );
		if ( curp == buf )
				return;
		curp = adjust(prevline(prevline(curp)-1), col);
		//if(!row) pagestart = curp+1;
}

void right(){
		if(curp < etxt) ++curp;
}

void wleft(){
		while (isspace(*(curp-1)) && buf < curp) --curp;
		while (!isspace(*(curp-1)) && buf < curp) --curp;
}

void pgdown(){
		pagestart = curp = prevline(pageend-1);
		while (0 < row--) down();
		pageend = etxt;
		clearscreen=1;
}

void pgup(){
		int i = termlines;
		while (0 < --i) { pagestart = prevline(pagestart-1); up(); }
		clearscreen=1;
}

void wright(){
		while (!isspace(*curp) && curp < etxt) ++curp;
		while (isspace(*curp) && curp < etxt) ++curp;
}

void lnbegin(){
		curp = prevline(curp);
}

void lnend(){
		curp = nextline(curp); left();
}

void top(){
		curp = buf;
		clearscreen=1;
}

void bottom(){
		pageend = curp = etxt;
}

void delete_one(){
		if(curp < etxt) {
				if(*curp == '\n') line_count--;
				if((char*)&undop[1] < ubuf+BUF) {
						undop->ch = *curp;
						undop->pos = -(POINTER)curp;        // negative means delete
						undop++;
				}
				cmove(curp+1, curp, etxt-curp);
				clearscreen=1;
		}
}

void bksp(){
		if(buf < curp) {
				left(); delete_one();
		}
}

void delrol(){
		if(*curp != '\n') 
				do { delete_one(); } while(curp < etxt && *curp != '\n');
		else 
				delete_one();
}

void delline(){
		lnbegin();
		delrol();delrol();
}

void undo(){
		if((void*)undop > (void*)ubuf) {
				undop--;
				curp = (char*)undop->pos;
				if(undop->pos < 0) {    // negative means was delete
						curp = (char*)-(POINTER)curp;              // so insert
						cmove(curp, curp+1, etxt-curp);
						*curp = undop->ch;
						if(*curp == '\n') line_count++;
				}
				else{   // was insert so delete
						if(*curp == '\n') line_count--;
						cmove(curp+1, curp, etxt-curp);
				} 
				clearscreen=1;
		}
}

void writef(){
		int i;
		write(i = creat(filename, MODE), buf, (int)(etxt-buf));
		close(i);
		undop = (U_REC*)&ubuf;
}

int prompt(char *prompt, char *s, char ch){
		char c;
		int i = strlen(s);
		gotoxy(17,1);
		clrtoeol();
		write1(prompt);
		write1(s);
		do {
				c = getch();
				if(c == 127 ) { // delete
						if(!i) continue;
						i--; emitch('\b'); emitch(' '); emitch('\b');
				}
				else {
						if(i == PROMPTMAX-1) continue;
						s[i++] = c;
				}
				if(c != 0x1b) emitch(c);
		} while(c != 0x1b && c != '\n' && c != '\r' && c != ch );
		s[--i] = 0;
		return(c == 0x1b ? 0 : i);
}

void findnext(){
		do { right(); } while(curp < etxt && scmp(curp,ss,strlen(ss)));
}

void findprev(){
		do { left(); } while(curp > buf && scmp(curp,ss,strlen(ss)));
}

void find(){
		int i = prompt(" Look for: ", ss, 0x0c);
		if(i) 
				findnext();
}

void goln(int i){
		top(); while(--i > 0) down();
}

void gotoln(){
		if(prompt(" goto line: ", ln, 0x0a)) goln(atoi(ln));
}

void quit(){
		char c;
		if((void*)undop > (void*)ubuf) {
				gotoxy(3,1);
				writestr(" File changed. Save? y/n ");
				do{
						c = getch();
				} while ( !(( c=='y' ) || ( c=='n' ) ) );
				if( c == 'y') writef();
		}
		done = 1;
}

void nop(){}
//#pragma GCC push_options
//#pragma GCC optimize("O0")

void help(){
		char const *s  = helpt;
		gotoxy(1,1);
		while(*s) { emitch(*s); *s++ == '\n' ? clrtoeol() : nop(); }
		getkey(); 
		clearscreen=1;
}

//#pragma GCC pop_options



void redraw(){
		int i=0, j=0;
		char status[MAXCOLUMNS];
		char* p = curp;
		int l = 1;
		debug("r: curp: %x, pagestart: %x, pageend: %x, clearscreen: %u\n", curp, pagestart, pageend, clearscreen );

		if(curp < pagestart) {
				pagestart = prevline(curp);
				clearscreen=1;
		}
		if(pageend <= curp) {
				pagestart = prevline(curp);
				i = termlines;
				while (--i) 
						pagestart = prevline(pagestart-1);
				clearscreen=1;
		}
		if(clearscreen) {
				debug("clearscreen\n");
				writestr("\033[H\033[J\n"); 
				clearscreen = 0;
		}      // clear screen
		pageend = pagestart;
		gotoxy(1,2);
		while (1) {
				if(curp == pageend) {
						row = i; col = j;
				}
				if(*pageend == '\n') {
						++i; j = 0;
				}
				if(i >= termlines || line_count <= i || etxt <= pageend) 
						break;
				if(*pageend != '\r') {
						if(termcolumns > j) 
								emitch(*pageend);
						j += *pageend == '\t' ? TABSZ-(j & (TABM)) : *pageend == '\n' ? 0 : 1;
				}
				++pageend;
		}
		emitch('\n');// also show the last line, if not ending with \n
		i = xy.Y;
		while(i <= termlines+1) { clrtoeol(); gotoxy(1,i++); }
		// draw status line
		highlight(1);
		while(p > buf) { 
				if(*--p == '\n') 
						l++; 
		};
		gotoxy(1,1);
		clrtoeol();
		sprintf(status, 
#ifdef TM
"  %3d %3d %c h:help j:left k:down i:up l:right f:find q:quit", 
#else
#ifdef OSX
"  %6d %5d %c Alt+H: Help / Alt+W: Save / Alt+F: Find / Alt+G: Goto Line /  Alt+Q: Quit", 
#else
"  %6d %5d %c F1: Help / F2: Save / F5: Find / F8: Goto Line /  F10: Quit", 
#endif
#endif
			l, col+1, ((void*)undop > (void*)ubuf) ? '*' : ' ');
		write1(status);
		highlight(0);

		gotoxy((termcolumns > col+1) ? col+1 : termcolumns, row+2);
}



#ifndef TM


//misc v // escape sequences and keycodes
// Not sure now. Doesn't work on osx; either cause x64 or endianness
#ifdef OSX
int keycodetable[] = {
		0xff00445b, 0xff00425b, 0xff00415b, 0xff00435b,   //Cursor left,down,up,right
		0x323b315b, 0xa4, 0xbc, 0x323b315b, // shift+left (wordleft), pgdown:ä , pgup:ü, shift+right (wordright) (shift-keft/right doesnt work)
		// ctrl+left = 44353b31; ctrl+right = 43353b31
		0xff000062, 0xff000066, 0x84, 0x9c,   // home, end, Ä,Ü
		0xff007e33, 0x7f, 0x82, 0xa8,          // Del, Backspace, Alt+d, ALT+U
		0x91, 0x92, 0x7e , 0x80,  // Alt+ w f n p
		//0xff00514f, 0x7e35315b, 0x7e37315b , 0x7e38315b,  // F2, F5, F6, F7
		0xa9,  0xaa, 0xae, 0xab,  // F8, F1, ALT+R, ALT+Q
		0x82, // alt+d
		0
};
#else
int keycodetable[] = {
		0xff000044, 0xff000042, 0xff000041, 0xff000043,   //Cursor left,down,up,right
		0x44333b31, 0xff007e36, 0xff007e35, 0x43333b31, // alt+left, pgdown, pgup, alt+right
		// ctrl+left = 44353b31; ctrl+right = 43353b31
		0xff000048, 0xff000046, 0x48353b31, 0x46353b31,   // home, end, ctrl+home, ctrl+end
		0xff007e33, 0x7f, 0xff000000, 'U' & 0x1f,          // Del, Backspace, Alt+Del, Ctrl+U
		0xff000051, 0xff7e3531, 0xff7e3731 , 0xff7e3831,  // F2, F5, F6, F7
		0xff7e3931,  0xff000050, 'L' & 0x1f, 0xff7e3132,  // F8, F1, ctrl+L, F10
		0x7e333b33, // alt+del
		0
};
#endif
//misc ^


// command key function array, one to one correspondence to key array, above
void (*func[]) () = {
		left, down, up, right,
		wleft, pgdown, pgup, wright,
		lnbegin, lnend, top, bottom,
		delete_one, bksp, delrol, undo,
		writef, find, findnext, findprev,
		gotoln,	help, redraw, quit, 
		delline,
		nop
};



#else

char keycodetable[] = "jkil" 
"JmKoIL"
"OM"
"\x7f" "u," 
"#"

"wfnp"
"ghr"
"qd";

// command key function array, one to one correspondence to key array, above
void (*func[]) () = {
		left, down, up, right,
		wleft, pgdown, pgdown, pgup, pgup, wright,
		top, bottom,
		bksp, undo, delete_one,
		switchmode,
		//		lnbegin, lnend, top, bottom,
//		delete_one, bksp, delrol, undo,
		writef, find, findnext, findprev,
		gotoln,	help, redraw, 
		quit, 
		delline,
		nop
};
#endif





int main(int argc, char **argv){
		int i=0;
#ifdef TM
		char keycode;
#else
		int keycode;
#endif

		if (argc < 2) {
				fprintf(stderr, "usage: %s <filename>\n",argv[0]); return (2);
		}

#ifdef dodebug
		dbg = open("debug.txt", O_WRONLY | O_CREAT | O_APPEND, 0600);
		debug("Ok.\n");
#endif

		filename = *++argv;
		debug("Filemname: %s\n", *argv);

		/*	{
				char* p = strchr(*argv, ':');
				if(p) { *p++ = '\0'; j = atoi(p); }
				}*/
		if(0 < (i = open(filename = *argv, O_RDONLY,0))) {
				debug("Opened, i: %d\n",i);
				etxt += read(i, (POINTER*)buf, BUF);
				if(etxt < buf) etxt = buf;
				else{
						char *p = etxt;
						while(p > buf) if(*--p == '\n') line_count++;
				}
				close(i);
		} else { // Create file
				debug("Create file\n");
				if ( 0< (i=creat(filename=*argv,0600)) ){
						etxt=buf;
				} else {
						fprintf(stderr, "Couldn't open or create %s\n", filename);
						return(3);
				}
		}

		GetSetTerm(0);
		getwinsize();
		goln(1);

		while (!done) {
				redraw();
#ifdef TM
				do {
						keycode = getkey();
						if ( keycode == '#' ) mode=!mode;
				} while ( keycode == '#' );
				if ( !mode ){
						i = 0;
						while (keycodetable[i] != keycode && keycodetable[i] != 0 ) ++i;
						(*func[i])();
				} else {
#else
				keycode = getkey();
				i = 0;
				while (keycodetable[i] != keycode && keycodetable[i] != 0 ) ++i;
				(*func[i])();
						if(!((keycodetable[i]!=0) || (keycode & 0xFFFFFF80)) ) { // no extended keycode
#endif
						//if(!(keycodetable[i]!=0)  && (mode==1)){
								if(etxt < ebuf) {
										cmove(curp, curp+1, etxt-curp);
										*curp = (char)keycode == '\r' ? '\n' : (char)keycode;
										if(*curp++ == '\n') {
												line_count++; 
												clearscreen = 1;
										}
										if((char*)&undop[1] < ubuf+BUF) {
												//					undop->ch = curp[-1];
												undop->pos = (POINTER)curp-1;       // positive means insert
												undop++;
										}
								}
						}
		}
		gotoxy(1,termlines+2);
		GetSetTerm(1);
		return (0);
}
