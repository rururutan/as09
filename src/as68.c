/************************************************
 *						*
 *	MC6800/6801/6803/6809 cross assembler	*
 *						*
 ************************************************/

#include	<stdio.h>
#include	"as68.h"

char *malloc(),*strcpy(),*strcat();

#define	MAXOPTIM	2000

int *shortform,sfp,nchange;

extern OPTABLE optab[], *ophash[];
extern int offset[3][4];

OPTABLE	*oprptr;

LBLTABLE *label,*lblptr;

int	debug,optimize,optcount,object,list,verbos,symbol,xref;

int	lineno,lc,llc,objlc,dp,errors,labels,pass,indirect,valid,
	postf,endfile,startadr,byte,word,pos,objpos,chksum,mc6801;

char	linebuf[MAXCHAR],*lineptr;

char	objbuf[OBJSIZE];

FILE	*srcf,*lstf,*objf;

/* use for library inclusion */
FILE *filestk[MAXLIB];
int filesp;

char	srcfile[FNAMESZ] = "",
	lstfile[FNAMESZ] = "",
	objfile[FNAMESZ] = "";

#ifdef	MC6809
char	title[] = "MC6809 cross assembler version 01.00\n";
#else
char	title[] = "MC6800/6801/6803 cross assembler version 01.00\n";
#endif

FILE *popfile();

main(argc,argv)
int argc;
char **argv;
{char *av0;
register i;
	av0=argv[0];
	fprintf(stderr,title);
	for ( i = 1; i < argc; i++ ) {
		if ( *argv[i] == '-' ) {
			switch ( *++argv[i] ) {
#ifdef	MC6809
			case 'O':
				if ( (shortform = (int *)malloc(sizeof(int)*MAXOPTIM)) == NULL ) {
					fprintf(stderr,"no core for optimization\n");
					break;
				}
				optimize++;
				break;
#endif
			case 'd':
				debug++;
				break;
			case 'l':
				list++;
				if ( !argv[i+1] || *argv[i+1] == '-' ) continue;
				if ( *argv[++i] )
					strcpy(lstfile,argv[i]);
				break;
			case 'o':
				object++;
				if ( !argv[i+1] || *argv[i+1] == '-' ) continue;
				if ( *argv[++i] )
					strcpy(objfile,argv[i]);
				break;
			case 's':
				symbol++;
				break;
			case 'v':
				verbos++;
				break;
			case 'x':
				xref++;
				break;
#ifndef	MC6809
			case 'X':
				mc6801++;
				break;
#endif
			case 0:
				break;
			default:
				fprintf(stderr,"%s: illegal option -%c\n",av0,*argv[i]);
				exit(1);
			}
		}
		else strcpy(srcfile,argv[i]);
	}	
	if ( !*srcfile ) {
#ifdef	MC6809
		fprintf(stderr,"usage: %s src_file [-O] [-v] [-s] [-o [obj_file]] [-l [lst_file]]\n",av0);
#else
		fprintf(stderr,"usage: %s src_file [-X] [-v] [-s] [-o [obj_file]] [-l [lst_file]]\n",av0);
#endif
		exit(1);
	}
	if ( !*objfile ) {
		strcpy(objfile,srcfile);
		strcat(objfile,".o");
	}
	if ( !*lstfile ) {
		strcpy(lstfile,srcfile);
		strcat(lstfile,".l");
	}
	initialize();
	pass = 1;
DEBUG(fprintf(stderr,"enter pass 1\n"))
DEBUG(fprintf(stderr,"open %s\n",srcfile))
	if ( (srcf = fopen(srcfile,"r")) == NULL ) fileerror(srcfile);
	if ( verbos ) fprintf(stderr,"%s:\n",srcfile);
	assemble();
	if ( optimize ) {
		pass = -1;
DEBUG(fprintf(stderr,"enter pass 1.5\n"))
DEBUG(fprintf(stderr,"open %s\n",srcfile))
		do {
			if ( (srcf = fopen(srcfile,"r")) == NULL ) fileerror(srcfile);
			if ( verbos ) fprintf(stderr,"%s:\n",srcfile);
			assemble();
		} while ( nchange );
	}
	pass = 2;
DEBUG(fprintf(stderr,"enter pass 2\n"))
DEBUG(fprintf(stderr,"open %s\n",srcfile))
	if ( (srcf = fopen(srcfile,"r")) == NULL ) fileerror(srcfile);
	if ( verbos ) fprintf(stderr,"%s:\n",srcfile);
DEBUG(fprintf(stderr,"open %s\n",lstfile))
	if ( list || symbol )
		if ( (lstf = fopen(lstfile,"w")) == NULL ) fileerror(lstfile);
DEBUG(fprintf(stderr,"open %s\n",objfile))
	if ( object )
		if ( (objf = fopen(objfile,"w")) == NULL ) fileerror(objfile);
	assemble();
DEBUG(fprintf(stderr,"close all files\n"))
	if ( object ) fclose(objf);
	if ( list || symbol ) fclose(lstf);
	exit(0);
}

fileerror(s)
char *s;
{
#ifdef	MC6809
	fprintf(stderr,"as09: can't open %s\n",s);
#else
	fprintf(stderr,"as68: can't open %s\n",s);
#endif
	exit(1);
}

hash( s )
register char *s;
{register int h;
	h = 0;
	while ( *s ) h = h * 5 + *s++;
	return (h & 0xff);
}

initialize()
{LBLTABLE *getnode();
 register OPTABLE *p, *q;
 register int h;
	if ( (label = getnode()) == NULL ) {
		fprintf(stderr,"fatal error: can't alloc label node\n");
		exit(1);
	}
	label->name[0] = '\0';
	filesp = 0;
	for ( p = optab; *p->mnemonic; p++ ) {
		q = ophash[ h = hash( p->mnemonic ) ];
		if ( q ) {
			while ( q->nl != NULL ) q = q->nl;
			q->nl = p;
		}
		else {
			ophash[ h ] = p;
		}
		p->nl = NULL;
	}
}

char *getline()
{	lineptr = linebuf + LINEHEAD;
	return fgets(lineptr,MAXCHAR-LINEHEAD,srcf);
}

assemble()
{
	initpass();
	while ( !endfile ) {
		if ( getline() == NULL ) {
			fclose(srcf);
			if ( (srcf = popfile()) == NULL ) break;
			continue;
		}
		initline();
		if ( *lineptr == '*' ) clearaddress();
		else {
			if ( !isspace(*lineptr) ) deflabel();
			operation();
		}
		putline();
	}
	terminate();
}

initpass()
{
	lineno = errors = lc = dp = endfile = objlc = objpos = optcount = sfp = nchange = 0;
	if ( verbos ) fprintf(stderr,pass == -1 ? "<pass 1.5>\n" : "<pass %d>\n",pass);
}

initline()
{register char *p;
	lblptr = (LBLTABLE *)0;
	postf = 0;
	pos = 10;
	for ( p = linebuf; p < lineptr; p++) *p = ' ';
	printdecimal(++lineno,linebuf);
	printaddress(llc = lc);
}

char *getlabel(buf)
char *buf;
{register char *p;
	if ( !isalpha(*lineptr) ) error("illegal character in label");
	for ( p = buf; p < buf + LBLSIZE; p++,lineptr++ )
		if ( !isalnum(*p = *lineptr) ) break;
	while ( isalnum(*lineptr) ) lineptr++;
	*p = '\0';
	return buf;
}

LBLTABLE *reflabel(lbl)
char *lbl;
{register LBLTABLE *lp;
 register i;
	for ( lp = label; lp != NULL; lp = (i < 0) ? lp->right : lp->left) {
		if ( (i = strcmp(lbl,lp->name)) == 0 ) return lp;
	}
	return NULL;
}

LBLTABLE *getnode()
{static	LBLTABLE *p = NULL;
 static i = 0;
	if ( p == NULL || i >= MAXLABEL ) {
		if ( (p = (LBLTABLE *)malloc(sizeof(LBLTABLE) * MAXLABEL )) == NULL )
			return NULL;
		i = 0;
DEBUG(fprintf(stderr,"alloc %d nodes\n",MAXLABEL))
	}
	return (p + i++);
}

deflabel()
{char temp[LBLSIZE + 1];
 register LBLTABLE *lp;
 register i;
	if ( *lineptr == '\n' ) return;
	getlabel(temp);
	lp = label;
	while ( 1 ) {
		if ( (i = strcmp(temp,lp->name)) == 0 ) {
			if ( lp->line != lineno )
				error("multiple defined label");
			(lblptr = lp)->value = llc;
			return;
		}
		if ( i < 0 ) {
			if ( lp->right != NULL ) {
				lp = lp->right;
			}
			else {
				lp = (lp->right = getnode());
				break;
			}
		}
		else {
			if ( lp->left != NULL ) {
				lp = lp->left;
			}
			else {
				lp = (lp->left = getnode());
				break;
			}
		}
	}
	if ( lp == NULL ) {
		fprintf(stderr,"label table overflow\n");
		exit(1);
	}
	labels++;
	(lblptr = lp)->value = llc;
	lp->line = lineno;
	strcpy(lp->name,temp);
	lp->right = lp->left = NULL;
	return;
}

operation()
{char temp[MNEMOSIZE + 1];
 register char *p, *pp;
 register OPTABLE *q;
	skipspace();
	if ( *lineptr == '\n' ) return;
	if ( !isalpha(*lineptr) ) {
		error("illegal chararcter in mnemonic");
DEBUG(fprintf(stderr,"*lineptr : %c\n",*lineptr))
		return;
	}
	for ( p = temp, pp = temp + MNEMOSIZE; p < pp; p++,lineptr++ )
		if ( !isalnum(*p = toupper(*lineptr)) ) break;
	*p = '\0';
	while ( isalnum(*lineptr) ) lineptr++;
	for ( q = ophash[ hash( temp ) ]; q != NULL; q = q->nl ) {
		if ( strcmp(temp,q->mnemonic) == 0 ) {
#ifndef	MC6809
			if ( !mc6801 && q->option == MC6801 ) break;
#endif
			oprptr = q;
			(*q->process)();
			if ( *lineptr != '\n' && !isspace(*lineptr) ) error("extra garbage");
			return;
		}
	}
	error("unrecognizable mnemonic");
}

skipspace()
{	while ( isspace(*lineptr) ) lineptr++;
}

#ifdef	MC6809
operand(grp,mode)
int grp,mode;
{int val,reg;
	skipspace();
	if ( (mode & (IMMEDIATE | IMMEDIATE2)) && checkchar('#') )
	{	putcode(grp,IMMEDIATE_MODE);
		val = expression();
		if ( mode & IMMEDIATE )
		{	if ( val < -128 || 255 < val )
				error("value range error");
			putbyte(val);
		}
		else putword(val);
		return;
	}
	indirect = checkchar('[');
	if ( (mode & INDEX) && checkchar(',') )
	{	putcode(grp,INDEX_MODE);
		if ( checkchar('-') )
		{	if ( checkchar('-') ) index(0x83,getreg(INDEXREG));
			else if ( indirect ) error("illegal indirect mode");
			else index(0x82,getreg(INDEXREG));
		}
		else
		{	reg = getreg(INDEXREG);
			if ( checkchar('+') )
				if ( checkchar('+') ) index(0x81,reg);
				else if ( indirect )
						error("illegal indirect mode");
				else index(0x80,reg);
			else index(0x84,reg);
		}
	}
	else if ( (mode & INDEX) && checkreg('A') )
		if ( checkchar(',') )
		{	putcode(grp,INDEX_MODE);
			index(0x86,getreg(INDEXREG));
		}
		else error("invalid accumulator offset");
	else if ( (mode & INDEX) && checkreg('B') )
		if ( checkchar(',') )
		{	putcode(grp,INDEX_MODE);
			index(0x85,getreg(INDEXREG));
		}
		else error("invalid accumulator offset");
	else if ( (mode & INDEX) && checkreg('D') )
		if ( checkchar(',') )
		{	putcode(grp,INDEX_MODE);
			index(0x8b,getreg(INDEXREG));
		}
		else error("invalid accumulator offset");
	else
	{	val = expression();
		if ( (mode & INDEX) && checkchar(',') )
		{	putcode(grp,INDEX_MODE);
			switch ( reg = getreg(INDEXREG | PC | PCR) )
			{ case X: case Y: case U: case S:
				if ( valid && -16 <= val && val <= 15 && !indirect)
					index(val ? (val & 0x1f) : 0x84,reg);
				else if ( checkbyte(val) )
				{	index(0x88,reg);
					putbyte(val);
				}
				else
				{	index(0x89,reg);
					putword(val);
				}
				break;
			case PC:
				if ( checkbyte(val) )
				{	index(0x8c,0);
					putbyte(val);
				}
				else
				{	index(0x8d,0);
					putword(val);
				}
				break;
			case PCR:
				if ( checkbyte(val -= llc + 3 +
						(oprptr->prefix ? 1 : 0)) )
				{	index(0x8c,0);
					putbyte(val);
				}
				else
				{	index(0x8d,0);
					putword(val - 1);
				}
			}
		}
		else if ( (mode & INDEX) && indirect )
		{	putcode(grp,INDEX_MODE);
			postbyte(0x9f);
			putword(val);
		}
		else if ( (mode & DIRECT) && (byte || 
			((unsigned)(val - (dp << 8)) <= 255 && valid && !word)) )
		{	putcode(grp,DIRECT_MODE);
			putbyte(val - (dp << 8));
		}
		else if ( mode & EXTEND )
		{	putcode(grp,EXTEND_MODE);
			putword(val);
		}
		else error("illegal addressing mode");
	}
	if ( indirect && !checkchar(']') ) error("missing ']'");
}

index(frame,reg)
int frame,reg;
{int xr;
	switch ( reg )
	{	case X: xr = 0x00; break;
		case Y: xr = 0x20; break;
		case U: xr = 0x40; break;
		case S: xr = 0x60; break;
		default: xr = 0;
	}
	postbyte(frame | xr | (indirect ? 0x10 : 0));
}
#else
operand(grp,mode)
int grp,mode;
{int val;
	skipspace();
	if ( (mode & (IMMEDIATE | IMMEDIATE2)) && checkchar('#') )
	{	putcode(grp,IMMEDIATE_MODE);
		val = expression();
		if ( mode & IMMEDIATE )
		{	if ( val < -128 || 255 < val )
				error("value range error");
			putbyte(val);
		}
		else putword(val);
		return;
	}
	if ( (mode & INDEX) && checkchar(',') && checkchar('X') )
	{	putcode(grp,INDEX_MODE);
		putbyte( 0 );
		return;
	}
	val = expression();
	if ( (mode & INDEX) && checkchar(',') && checkchar('X') )
	{	putcode(grp,INDEX_MODE);
		if ( val < 0 || 255 < val )
			error( "offset out of range" );
		putbyte( val );
	}
	else if ( (mode & DIRECT) && (byte || 
		((unsigned)(val - dp) <= 255 && valid && !word)) )
	{	putcode(grp,DIRECT_MODE);
		putbyte(val - dp);
	}
	else if ( mode & EXTEND )
	{	putcode(grp,EXTEND_MODE);
		putword(val);
	}
	else error("illegal addressing mode");
}
#endif

expression()
{register int val;
	valid = 1;
	byte = word = 0;
	if ( checkchar('<') ) byte = 1;
	else if ( checkchar('>') ) word = 1;
	val = term();
	while ( 1 )
	{	switch ( *lineptr++ )
		{	case '+': val += term(); continue;
			case '-': val -= term(); continue;
			case '*': val *= term(); continue;
			case '/': val /= term(); continue;
			case '%': val %= term(); continue;
			case '&': val &= term(); continue;
			case '|': val |= term(); continue;
			case '^': val ^= term(); continue;
			case '<': val <<= term(); continue;
			case '>': val >>= term(); continue;
			default: break;
		}
		break;
	}
	switch ( *--lineptr )
	{	case ' ': case '\t': case ',':
		case ')': case ']': case '\n':
			break;
		default: error("illegal character in expression");
DEBUG(fprintf(stderr,"*lineptr : %c\n",*lineptr))
	}
/*	if ( byte && (val < -128 || 127 < val) ) error("value range error");
*/
	return val;
}

term()
{register int tv;
 char temp[LBLSIZE + 1];
 register LBLTABLE *lp;
	switch ( *lineptr++ )
	{	case '-': return -term();
		case '~': return ~term();
		case '*': return llc;
		case '\'': return *lineptr++;
		case '$':
			for ( tv = 0; isxdigit(*lineptr); lineptr++)
				tv = tv * 16 +
					(isdigit(*lineptr) ? (*lineptr - '0') :
					  (toupper(*lineptr) - 'A' + 10));
				return tv;
		case '%':
			for ( tv = 0;
				*lineptr == '0' || *lineptr == '1'; lineptr++)
					tv = tv * 2 + *lineptr - '0';
				return tv;
		case '(':
			tv = expression();
			if ( !checkchar(')') ) error("missing ')'");
			return tv;
		default:
			if ( isalpha(*--lineptr) )
			{	if ( lp = reflabel(getlabel(temp)) )
				{	if ( lineno < lp->line ) valid = 0;
					return (lp->value);
				}
				error("undefined label");
				return (valid = 0);
			}
			else if ( isdigit(*lineptr) )
			{	for ( tv = 0; isdigit(*lineptr); lineptr++)
					tv = tv * 10 + *lineptr - '0';
				return tv;
			}
			else
			{	error("illegal character in term");
DEBUG(fprintf(stderr,"*lineptr : %c\n",*lineptr))
				return (valid = 0);
			}
	}
}

error(s)
char *s;
{	errors++;
	if ( pass != 2 ) return;
	fprintf(stderr,"%5d: %s\n",lineno,s);
	if ( list ) fprintf(lstf,"*** %s\n",s);
}

none()
{	putcode(GROUP0,NO_MODE);
}

load()
{	operand(GROUP2,LOAD);
}

load2()
{	operand(GROUP2,LOAD2);
}

ccr()
{	operand(GROUP0,IMMEDIATE);
}

memory()
{	operand(GROUP1,MEMORY);
}

branch()
{int val;
	skipspace();
	if ( (val = expression() - llc - 2) < -128 || 127 < val )
		error("short branch too far");
	putcode(GROUP0,NO_MODE);
	putbyte(val);
}

lbranch()
{int val;
 OPTABLE shortop;
	skipspace();
	switch ( pass ) {
	case 1:
		if ( optimize && sfp < MAXOPTIM ) {
			shortform[sfp++] = llc;
		}
		putcode(GROUP0,NO_MODE);
		putword(0);
		return;
	case -1:
		if ( optimize && sfp < MAXOPTIM ) {
			if ( shortform[sfp] == -1 ) {
				sfp++;
				putword(0);
				return;
			}
			if ( -128 <= (val = expression() - shortform[sfp] -2) && val <= 127 ) {
				nchange++;
				shortform[sfp++] = -1;
				putword(0);
				return;
			}
			shortform[sfp++] = llc;
		}
		putcode(GROUP0,NO_MODE);
		putword(0);
		return;
	case 2:
		if ( optimize && sfp < MAXOPTIM && shortform[sfp++] == -1 ) {
			printchar('>',4);
			optcount++;
			shortop.prefix = 0;
			shortop.opcode = (oprptr->opcode == 0x16 ? 0x20 :
				oprptr->opcode == 0x17 ? 0x8d : oprptr->opcode);
			oprptr = &shortop;
			putcode(GROUP0,NO_MODE);
			putbyte(expression() - llc - 2);
			return;
		}
		putcode(GROUP0,NO_MODE);
		putword(expression() - llc - (oprptr->prefix ? 4 : 3));
		return;
	}
}

transfer()
{int r1,r2;
	skipspace();
	putcode(GROUP0,NO_MODE);
	if ( (r1 = regno()) < 0 ) error("register required");
	if ( !checkchar(',') ) error("missing ','");
	if ( (r2 = regno()) < 0 ) error("register required");
	if ( (r1 ^ r2) & 0x08 ) error("invalid register combination");
	putbyte((r1 << 4) | r2);
}

regno()
{	switch ( getreg(ALLREG) )
	{	case D: return 0;
		case X: return 1;
		case Y: return 2;
		case U: return 3;
		case S: return 4;
		case PC: return 5;
		case A: return 8;
		case B: return 9;
		case CC: return 10;
		case DP: return 11;
		default:
			error("funny register");
			return 0;
	}
}
	
store()
{	operand(GROUP2,STORE);
}

lea()
{	operand(GROUP0,INDEX);
}

stacks()
{	pushpul(0);
}

stacku()
{	pushpul(1);
}

pushpul(u)
int u;
{register int m1,m2;
	m1 = 0;
	skipspace();
	do
	{	switch ( getreg(ALLREG) )
		{	case CC: m2 = 0x01; break;
			case A:  m2 = 0x02; break;
			case B:  m2 = 0x04; break;
			case D:  m2 = 0x06; break;
			case DP: m2 = 0x08; break;
			case X:  m2 = 0x10; break;
			case Y:  m2 = 0x20; break;
			case S: if ( !u ) error("invalid register");
				 m2 = 0x40; break;
			case U: if ( u ) error("invalid register");
				 m2 = 0x40; break;
			case PC: m2 = 0x80;
		} if ( m1 & m2 ) error("same register");
		m1 |= m2;
	} while ( checkchar(',') );
	putcode(GROUP0,NO_MODE);
	putbyte(m1);
}

org()
{int origin;
	skipspace();
	origin = expression();
	if ( !valid ) error("invalid expression");
	printaddress(lc = origin);
	flushobj();
}

rmb()
{int bytes;
	skipspace();
	bytes = expression();
	if ( !valid ) error("invalid expression");
	flushobj();
	printword(bytes,15);
	lc += bytes;
}

equ()
{int val;
	skipspace();
	val = expression();
	if ( !valid ) error("invalid expression");
	lblptr->value = val;
	clearaddress();
	printword(val,15);
}

fcb()
{int val;
	skipspace();
	do
	{	if ( checkchar('"') )
			while ( ((*lineptr != '"') || (*++lineptr == '"')) &&
				(*lineptr != '\n')) put1byte(*lineptr++);
		else
		{	val = expression();
			if ( val < -128 || 255 < val )
				error("value range error");
			put1byte(val);
		}
	} while ( checkchar(',') );
}

fdb()
{	skipspace();
	do put1word(expression()); while ( checkchar(',') );
}

fcc()
{char c;
	skipspace();
	if ( isalnum(c = *lineptr++) ) error("illegal delimter");
	while ( *lineptr != c )
	{	if ( *lineptr == '\n' )
		{	error("missing delimiter");
			return;
		}
		else put1byte(*lineptr++);
	}
	lineptr++;
}

nam()
{
}

endop()
{	
	clearaddress();
	fclose(srcf);
	if ( (srcf = popfile()) != NULL ) return;
	endfile = 1;
	skipspace();
	if ( *lineptr != '\n' )
	{	startadr = expression();
		printword(startadr,15);
	}
}

library()
{char fname[FNAMESZ];
 int i;
 FILE *fp;
	clearaddress();
	skipspace();
	for ( i = 0; i < FNAMESZ; i++, lineptr++ ) {
		if ( *lineptr == '\n' || isspace(*lineptr) ) break;
		fname[i] = *lineptr;
	}
	if ( i >= FNAMESZ ) {
		error("file name too long");
		return;
	}
	fname[i] = '\0';
	if ( verbos ) fprintf(stderr,"%s:\n",fname);
	if ( (fp = fopen(fname,"r")) == NULL ) fileerror(fname);
	pushfile(srcf);
	srcf = fp;
}
	
pushfile(fp)
FILE *fp;
{	
	if ( filesp >= MAXLIB ) error("file stack overflow");
	filestk[filesp++] = fp;
}

FILE *popfile()
{	if ( filesp <= 0 ) return NULL;
	return filestk[--filesp];
}

page()
{
}

setdp()
{unsigned newdp;
	skipspace();
	clearaddress();
	newdp = expression();
	if ( !valid /*|| 255 < newdp*/ ) error("invalid expression");
	printbyte(dp = newdp,15);
}

spc()
{
}

checkbyte(b)
int b;
{	return ( byte || (-128 <= b && b <= 127 && valid && !word) );
}

checkchar(c)
char c;
{	if ( toupper(*lineptr) == c )
	{	lineptr++;
		return 1;
	}
	return 0;
}

getreg(r)
int r;
{int reg;
	reg = 0;
	if ( checkreg('A') ) reg = A;
	else if ( checkreg('B') ) reg = B;
	else if ( checkreg('D') ) reg = D;
	else if ( checkreg('X') ) reg = X;
	else if ( checkreg('Y') ) reg = Y;
	else if ( checkreg('U') ) reg = U;
	else if ( checkreg('S') ) reg = S;
	else switch ( toupper(*lineptr++) )
	{	case 'C': if ( checkreg('C') ) reg = CC; break;
		case 'D': if ( checkreg('P') ) reg = DP; break;
		case 'P': if ( checkreg('C') ) { reg = PC; break; }
			else if ( checkchar('C') )
				if ( checkreg('R') ) reg = PCR;
				else lineptr--;
			break;
		default: lineptr--;
	}
	if ( r & reg ) return reg;
	error("illegal register");
	return 0;
}

checkreg(r)
char r;
{	if ( (toupper(*lineptr++) == r) && !isalnum(*lineptr) ) return 1;
	lineptr--;
	return 0;
}

putcode(grp,mode)
int grp,mode;
{	if ( oprptr->prefix )
	{	printbyte(putb(oprptr->prefix),10);
		printbyte(putb(oprptr->opcode + offset[grp][mode]),12);
	}
	else printbyte(putb(oprptr->opcode + offset[grp][mode]),10);
} 

postbyte(b)
int b;
{	postf = 1;
	printbyte(putb(b),15);
}

putbyte(b)
int b;
{	printbyte(putb(b),(postf ? 18 : 15));
}

putword(w)
int w;
{	printword(putw1(w),(postf ? 18 : 15));
}

putw1(w)
int w;
{	putb(w >> 8);
	putb(w);
	return w;
}

putb(b)
int b;
{	putobj(b);
	lc++;
	return b;
}

put1byte(b)
int b;
{	if ( 22 < pos + 3 ) flushline();
	printbyte(putb(b),pos);
	pos += 3;
}

put1word(w)
int w;
{	if ( 22 < pos + 5 ) flushline();
	printword(putw1(w),pos);
	pos += 5;
}

printchar(c,p)
char c;
int p;
{	linebuf[p] = c;
}

printdecimal(n,p)
int n;
char *p;
{register char *q;
	if ( pass != 2 ) return;
	q = p + 3;
	do
	{	*q-- = n % 10 + '0';
		n /= 10;
	} while ( n && p <= q );
}

clearaddress()
{register i;
	if ( pass != 2 ) return;
	for ( i = 5; i < 9; i++) linebuf[i] = ' ';
}

printaddress(a)
int a;
{	if ( pass != 2 ) return;
	printword(a,5);
}

printword(w,c)
int w,c;
{	if ( pass != 2 ) return;
	printbyte((w >> 8),c);
	printbyte(w,c+2);
}

printbyte(b,c)
int b,c;
{	if ( pass != 2 ) return;
	linebuf[c] = hexdigit(b >> 4);
	linebuf[c + 1] = hexdigit(b);
}

hexdigit(x)
int x;
{	return ( (x &= 0x0f) < 10 ? x + '0' : x - 10 + 'A');
}

flushline()
{register char *p;
	putline();
	for ( p = linebuf; p < linebuf + LINEHEAD; p++ ) *p = ' ';
	*p++ = '\n';
	*p = NULL;
	printaddress(lc);
	pos = 10;
}

putline()
{	if ( pass == 2 && list )
		fputs(linebuf,lstf);
}

terminate()
{	if ( pass != 2 ) return;
	if ( object ) termobj();
	if ( symbol ) dumpsymbol();
	printlog();
}

printlog()
{	if ( pass != 2 ) return;
	if ( list ) {
		fprintf(lstf,"\n     Total Errors %d\n",errors);
		if ( verbos ) fprintf(lstf,"     Total labels %d\n",labels);
	}
	fprintf(stderr,"\n     Total Errors %d\n",errors);
	if ( verbos ) fprintf(stderr,"     Total Labels %d\n",labels);
	if ( optimize ) fprintf(stderr,"     Total Optimized Branch %d\n",optcount);
}

putobj(b)
int b;
{	if ( OBJSIZE <= objpos ) flushobj();
	objbuf[objpos++] = b;
}

flushobj()
{int i;
	if ( pass == 2 && objpos > 0 && object )
	{	chksum = 0;
		fputs("S1",objf);
		put2hex(objpos + 3);
		put4hex(objlc);
		for ( i = 0; i < objpos; i++ ) put2hex(objbuf[i]);
		put2hex(~chksum);
		putc('\n',objf);
	}
	objpos = 0;
	objlc = lc;
}

termobj()
{	flushobj();
	fputs("S903",objf);
	chksum = 3;
	put4hex(startadr);
	put2hex(~chksum);
	putc('\n',objf);
}

put2hex(b)
int b;
{	if ( !object ) return;
	putc(hexdigit(b >> 4),objf);
	putc(hexdigit(b),objf);
	chksum += b;
}

put4hex(w)
int w;
{	put2hex(w >> 8);
	put2hex(w);
}

int lrf;

dumpsymbol()
{	if ( list ) fprintf(lstf,"\n");
	lrf = 1;
	printnode(label->left);
	if ( lrf % 3 ) fprintf(lstf,"\n");
}

printnode(lp)
LBLTABLE *lp;
{	if ( lp == NULL ) return;
	printnode(lp->right);
	fprintf(lstf,"%15s %4d %4x",lp->name,lp->line,lp->value);
	fprintf(lstf,lrf++ % 3 ? " " : "\n");
	printnode(lp->left);
}

isspace(c)
register int c;
{	return (c == ' ' || c == '\t' || c == '\f');
}

isalnum(c)
register int c;
{
/*	return (isalpha(c) || isdigit(c));
*/
	return (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')
			|| (c == '_') || (c == '.')
			|| ('0' <= c && c <= '9'));
}

isxdigit(c)
register int c;
{	return (isdigit(c) ||
		('A' <= c && c <= 'F') || ('a' <= c && c <= 'f'));
}

isalpha(c)
register int c;
{
/*	return (isupper(c) || islower(c) || (c == '_') || (c == '.'));
*/
	return (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')
			|| (c == '_') || (c == '.'));
}

isdigit(c)
register int c;
{	return ('0' <= c && c <= '9');
}

/*
isupper(c)
register int c;
{	return ('A' <= c && c <= 'Z');
}

islower(c)
register int c;
{	return ('a' <= c && c <= 'z');
}
*/

toupper(c)
register int c;
{
/*	if ( islower(c) )
*/
	if ( 'a' <= c && c <= 'z' )
		return c & 0xdf;
	return c;
}
