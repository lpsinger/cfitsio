/*  This file, putkey.c, contains routines that write keywords to          */
/*  a FITS header.                                                         */

/*  The FITSIO software was written by William Pence at the High Energy    */
/*  Astrophysic Science Archive Research Center (HEASARC) at the NASA      */
/*  Goddard Space Flight Center.  Users shall not, without prior written   */
/*  permission of the U.S. Government,  establish a claim to statutory     */
/*  copyright.  The Government and others acting on its behalf, shall have */
/*  a royalty-free, non-exclusive, irrevocable,  worldwide license for     */
/*  Government purposes to publish, distribute, translate, copy, exhibit,  */
/*  and perform such material.                                             */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#ifndef _FITSIO2_H
#include "fitsio2.h"
#endif

/*-------------------------------------------------------------------------*/
int ffprec(fitsfile *fptr,     /* I - FITS file pointer        */
           char *card,         /* I - string to be written     */
           int *status)        /* IO - error status            */
/*
  write a keyword record (80 bytes long) to the end of the header
*/
{
    char tcard[81];
    size_t len, ii;
    long nblocks;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    if ( (fptr->datastart - fptr->headend) == 80) /* only room for END card */
    {
        nblocks = 1;
        if (ffiblk(fptr, nblocks, 0, status) > 0) /* insert 2880-byte block */
            return(*status);  
    }

    strncpy(tcard,card,80);
    tcard[80] = '\0';

    len = strlen(tcard);
    for (ii=len; ii < 80; ii++)    /* fill card with spaces if necessary */
        tcard[ii] = ' ';

    for (ii=0; ii < 8; ii++)       /* make sure keyword name is uppercase */
        tcard[ii] = toupper(tcard[ii]);

    fftkey(tcard, status);        /* test keyword name contains legal chars */

    fftrec(tcard, status);          /* test rest of keyword for legal chars */

    ffmbyt(fptr, fptr->headend, IGNORE_EOF, status); /* move to end header */

    ffpbyt(fptr, 80, tcard, status);   /* write the 80 byte card */

    if (*status <= 0)
       fptr->headend += 80;           /* update end-of-header position */

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkys( fitsfile *fptr,     /* I - FITS file pointer        */
            char *keyname,      /* I - name of keyword to write */
            char *value,        /* I - keyword value            */
            char *comm,         /* I - keyword comment          */
            int  *status)       /* IO - error status            */
/*
  Write (put) the keyword, value and comment into the FITS header.
  The value string will be truncated at 68 characters which is the
  maximum length that will fit on a single FITS keyword.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffs2c(value, valstring, status);   /* put quotes around the string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkls( fitsfile *fptr,     /* I - FITS file pointer        */
            char *keyname,      /* I - name of keyword to write */
            char *value,        /* I - keyword value            */
            char *comm,         /* I - keyword comment          */
            int  *status)       /* IO - error status            */
/*
  Write (put) the keyword, value and comment into the FITS header.
  This routine is a modified version of ffpkys which supports the
  HEASARC long string convention and can write arbitrarily long string
  keyword values.  The value is continued over multiple keywords that
  have the name COMTINUE without an equal sign in column 9 of the card.
  This routine also supports simple string keywords which are less than
  69 characters in length.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];
    char tstring[FLEN_VALUE], *cptr;
    int next, remain, vlen, nquote, nchar, contin;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    remain = strlen(value);    /* number of characters to write out */
    next = 0;                  /* pointer to next character to write */
    
    /* count the number of single quote characters are in the string */
    nquote = 0;
    cptr = strchr(value, '\'');   /* search for quote character */

    while (cptr)  /* search for quote character */
    {
        nquote++;            /*  increment no. of quote characters  */
        cptr++;              /*  increment pointer to next character */
        cptr = strchr(cptr, '\'');  /* search for another quote char */
    }

    /* each quote character is expanded to 2 quotes, so leave enough space */
    nchar = 68 - nquote;    /*  max of 68 chars fit in a FITS string value */

    contin = 0;
    while (remain > 0)
    {
        strncpy(tstring, &value[next], nchar); /* copy string to temp buff */
        tstring[nchar] = '\0';
        ffs2c(tstring, valstring, status);  /* put quotes around the string */

        if (remain > nchar)   /* if string is continued, put & as last char */
        {
            vlen = strlen(valstring);
            nchar -= 1;        /* outputting one less character now */

            if (valstring[vlen-2] != '\'')
                valstring[vlen-2] = '&';  /*  over write last char with &  */
            else
            { /* last char was a pair of single quotes, so over write both */
                valstring[vlen-3] = '&';
                valstring[vlen-1] = '\0';
            }
        }

        ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/

        if (contin)           /* This is a CONTINUEd keyword */
            strncpy(card, "CONTINUE   ", 10);  /* overwrite the name and = */
 
        ffprec(fptr, card, status);  /* write the keyword */

        contin = 1;
        remain -= nchar;
        next  += nchar;
        nchar += 1;    /* this compensates for the earlier decrement */
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffplsw( fitsfile *fptr,     /* I - FITS file pointer  */
            int  *status)       /* IO - error status       */
/*
  Write the LONGSTRN keyword and a series of related COMMENT keywords
  which document that this FITS header may contain long string keyword
  values which are continued over multiple keywords using the HEASARC
  long string keyword convention.  If the LONGSTRN keyword already exists
  then this routine simple returns without doing anything.
*/
{
    char valstring[FLEN_VALUE], comm[FLEN_COMMENT];
    int tstatus;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    tstatus = 0;
    if (ffgkys(fptr, "LONGSTRN", valstring, comm, &tstatus) == 0)
        return(*status);     /* keyword already exists, so just return */

    ffpkys(fptr, "LONGSTRN", "OGIP 1.0", 
       "The HEASARC Long String Convention may be used.", status);

    ffpcom(fptr,
    "This FITS file may contain long string keyword values that are", status);

    ffpcom(fptr,
    "continued over multiple keywords.  The HEASARC convention uses the &",
    status);

    ffpcom(fptr,
    "character at the end of each substring which is then continued", status);

    ffpcom(fptr,
    "on the next keyword which has the name CONTINUE.", status);

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkyl( fitsfile *fptr,     /* I - FITS file pointer        */
            char *keyname,      /* I - name of keyword to write */
            int  value,         /* I - keyword value            */
            char *comm,         /* I - keyword comment          */
            int  *status)       /* IO - error status            */
/*
  Write (put) the keyword, value and comment into the FITS header.
  Values equal to 0 will result in a False FITS keyword; any other
  non-zero value will result in a True FITS keyword.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffl2c(value, valstring, status);   /* convert to formatted string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkyj( fitsfile *fptr,     /* I - FITS file pointer        */
            char *keyname,      /* I - name of keyword to write */
            long value,         /* I - keyword value            */
            char *comm,         /* I - keyword comment          */
            int  *status)       /* IO - error status            */
/*
  Write (put) the keyword, value and comment into the FITS header.
  Writes an integer keyword value.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffi2c(value, valstring, status);   /* convert to formatted string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkyf( fitsfile *fptr,      /* I - FITS file pointer                   */
            char  *keyname,      /* I - name of keyword to write            */
            float value,         /* I - keyword value                       */
            int   decim,         /* I - number of decimal places to display */
            char  *comm,         /* I - keyword comment                     */
            int   *status)       /* IO - error status                       */
/*
  Write (put) the keyword, value and comment into the FITS header.
  Writes a fixed float keyword value.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffr2f(value, decim, valstring, status);   /* convert to formatted string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkye( fitsfile *fptr,      /* I - FITS file pointer                   */
            char  *keyname,      /* I - name of keyword to write            */
            float value,         /* I - keyword value                       */
            int   decim,         /* I - number of decimal places to display */
            char  *comm,         /* I - keyword comment                     */
            int   *status)       /* IO - error status                       */
/*
  Write (put) the keyword, value and comment into the FITS header.
  Writes an exponential float keyword value.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffr2e(value, decim, valstring, status);   /* convert to formatted string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkyg( fitsfile *fptr,      /* I - FITS file pointer                   */
            char  *keyname,      /* I - name of keyword to write            */
            double value,        /* I - keyword value                       */
            int   decim,         /* I - number of decimal places to display */
            char  *comm,         /* I - keyword comment                     */
            int   *status)       /* IO - error status                       */
/*
  Write (put) the keyword, value and comment into the FITS header.
  Writes a fixed double keyword value.*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffd2f(value, decim, valstring, status);  /* convert to formatted string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkyd( fitsfile *fptr,      /* I - FITS file pointer                   */
            char  *keyname,      /* I - name of keyword to write            */
            double value,        /* I - keyword value                       */
            int   decim,         /* I - number of decimal places to display */
            char  *comm,         /* I - keyword comment                     */
            int   *status)       /* IO - error status                       */
/*
  Write (put) the keyword, value and comment into the FITS header.
  Writes an exponential double keyword value.*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    ffd2e(value, decim, valstring, status);  /* convert to formatted string */
    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkyt( fitsfile *fptr,      /* I - FITS file pointer        */
            char  *keyname,      /* I - name of keyword to write */
            long  intval,        /* I - integer part of value    */
            double fraction,     /* I - fractional part of value */
            char  *comm,         /* I - keyword comment          */
            int   *status)       /* IO - error status            */
/*
  Write (put) a 'triple' precision keyword where the integer and
  fractional parts of the value are passed in separate parameters to
  increase the total amount of numerical precision.
*/
{
    char valstring[FLEN_VALUE];
    char card[FLEN_CARD];
    char fstring[20], *cptr;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    if (fraction > 1. || fraction < 0.)
        return(*status = BAD_F2C);

    ffi2c(intval, valstring, status);  /* convert integer to string */
    ffd2f(fraction, 16, fstring, status);  /* convert to 16 decimal string */

    cptr = strchr(fstring, '.');    /* find the decimal point */
    strcat(valstring, cptr);    /* append the fraction to the integer */

    ffmkky(keyname, valstring, comm, card);  /* construct the keyword*/
    ffprec(fptr, card, status);  /* write the keyword*/

    return(*status);
}
/*-----------------------------------------------------------------*/
int ffpcom( fitsfile *fptr,      /* I - FITS file pointer   */
            char  *comm,         /* I - comment string      */
            int   *status)       /* IO - error status       */
/*
  Write 1 or more COMMENT keywords.  If the comment string is too
  long to fit on a single keyword (70 chars) then it will automatically
  be continued on multiple CONTINUE keywords.
*/
{
    char card[FLEN_CARD];
    int len, ii;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    len = strlen(comm);
    ii = 0;

    for (; len > 0; len -= 70)
    {
        strcpy(card, "COMMENT   ");
        strncat(card, &comm[ii], 70);
        ffprec(fptr, card, status);
        ii += 70;
    }

    return(*status);
}
/*-----------------------------------------------------------------*/
int ffphis( fitsfile *fptr,      /* I - FITS file pointer  */
            char  *history,      /* I - history string     */
            int   *status)       /* IO - error status      */
/*
  Write 1 or more HISTORY keywords.  If the history string is too
  long to fit on a single keyword (70 chars) then it will automatically
  be continued on multiple HISTORY keywords.
*/
{
    char card[FLEN_CARD];
    int len, ii;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    len = strlen(history);
    ii = 0;

    for (; len > 0; len -= 70)
    {
        strcpy(card, "HISTORY   ");
        strncat(card, &history[ii], 70);
        ffprec(fptr, card, status);
        ii += 70;
    }

    return(*status);
}
/*-----------------------------------------------------------------*/
int ffpdat( fitsfile *fptr,      /* I - FITS file pointer  */
            int   *status)       /* IO - error status      */
/*
  Write the DATE keyword into the FITS header.  If the keyword already
  exists then the date will simply be updated in the existing keyword.
*/
{
    char date[9], card[FLEN_CARD];
    time_t tp;
    struct tm *ptr;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    time(&tp);
    ptr = localtime(&tp);
    strftime(date, 9, "%d/%m/%y", ptr);

    strcpy(card, "DATE    =           '");
    strcat(card, date);
    strcat(card, "' / FITS file creation date (dd/mm/yy)");

    ffucrd(fptr, "DATE", card, status);

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkns( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            char *value[],      /* I - array of pointers to keyword values  */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Writes string keywords.
  The value strings will be truncated at 68 characters, and the HEASARC
  long string keyword convention is not supported by this routine.
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);
        if (repeat)
            ffpkys(fptr, keyname, value[ii], tcomment, status);
        else
            ffpkys(fptr, keyname, value[ii], comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpknl( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            int  *value,        /* I - array of keyword values              */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Writes logical keywords
  Values equal to zero will be written as a False FITS keyword value; any
  other non-zero value will result in a True FITS keyword.
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);

        if (repeat)
            ffpkyl(fptr, keyname, value[ii], tcomment, status);
        else
            ffpkyl(fptr, keyname, value[ii], comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpknj( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            long *value,        /* I - array of keyword values              */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Write integer keywords
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);
        if (repeat)
            ffpkyj(fptr, keyname, value[ii], tcomment, status);
        else
            ffpkyj(fptr, keyname, value[ii], comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpknf( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            float *value,       /* I - array of keyword values              */
            int decim,          /* I - number of decimals to display        */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Writes fixed float values.
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);
        if (repeat)
            ffpkyf(fptr, keyname, value[ii], decim, tcomment, status);
        else
            ffpkyf(fptr, keyname, value[ii], decim, comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkne( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            float *value,       /* I - array of keyword values              */
            int decim,          /* I - number of decimals to display        */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Writes exponential float values.
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);
        if (repeat)
            ffpkye(fptr, keyname, value[ii], decim, tcomment, status);
        else
            ffpkye(fptr, keyname, value[ii], decim, comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpkng( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            double *value,      /* I - array of keyword values              */
            int decim,          /* I - number of decimals to display        */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Writes fixed double values.
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);
        if (repeat)
            ffpkyg(fptr, keyname, value[ii], decim, tcomment, status);
        else
            ffpkyg(fptr, keyname, value[ii], decim, comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffpknd( fitsfile *fptr,     /* I - FITS file pointer                    */
            char *keyroot,      /* I - root name of keywords to write       */
            int  nstart,        /* I - starting index number                */
            int  nkey,          /* I - number of keywords to write          */
            double *value,      /* I - array of keyword values              */
            int decim,          /* I - number of decimals to display        */
            char *comm[],       /* I - array of pointers to keyword comment */
            int  *status)       /* IO - error status                        */
/*
  Write (put) an indexed array of keywords with index numbers between
  NSTART and (NSTART + NKEY -1) inclusive.  Writes exponential double values.
*/
{
    char keyname[FLEN_KEYWORD], tcomment[FLEN_COMMENT];
    int ii, jj, repeat, len;

    if (*status > 0)           /* inherit input status value if > 0 */
        return(*status);

    /* check if first comment string is to be repeated for all the keywords */
    /* by looking to see if the last non-blank character is a '&' char      */

    repeat = 0;
    len = strlen(comm[0]);

    while (len > 0  && comm[0][len - 1] == ' ')
       len--;                               /* ignore trailing blanks */

    if (comm[0][len - 1] == '&')
    {
        len = minvalue(len, FLEN_COMMENT);
        tcomment[0] = '\0';
        strncat(tcomment, comm[0], len-1); /* don't copy the final '&' char */
        repeat = 1;
    }

    for (ii=0, jj=nstart; ii < nkey; ii++, jj++)
    {
        ffkeyn(keyroot, jj, keyname, status);
        if (repeat)
            ffpkyd(fptr, keyname, value[ii], decim, tcomment, status);
        else
            ffpkyd(fptr, keyname, value[ii], decim, comm[ii], status);

        if (*status > 0)
            return(*status);
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffptdm( fitsfile *fptr, /* I - FITS file pointer                        */
            int colnum,     /* I - column number                            */
            int naxis,      /* I - number of axes in the data array         */
            long naxes[],   /* I - length of each data axis                 */
            int *status)    /* IO - error status                            */
/*
  write the TDIMnnn keyword describing the dimensionality of a column
*/
{
    char keyname[FLEN_KEYWORD], tdimstr[FLEN_VALUE], comm[FLEN_COMMENT];
    char value[80];
    int ii;

    if (*status > 0)
        return(*status);

    if (colnum < 1 || colnum > 999)
        return(*status = BAD_COL_NUM);

    if (naxis < 1)
        return(*status = BAD_DIMEN);

    ffkeyn("TDIM", colnum, keyname, status);      /* construct TDIMn name */

    strcpy(tdimstr, "(");            /* start constructing the TDIM value */   

    for (ii = 0; ii < naxis; ii++)
    {
        if (ii > 0)
            strcat(tdimstr, ",");   /* append the comma separator */

        if (naxes[ii] < 0)
            return(*status = BAD_NAXES);

        sprintf(value, "%ld", naxes[ii]);
        strcat(tdimstr, value);     /* append the axis size */
    }

    strcat(tdimstr, ")" );          /* append the closing parenthesis */

    strcpy(comm, "size of the multidimensional array");
    ffpkys(fptr, keyname, tdimstr, comm, status);  /* write the keyword */
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffphps( fitsfile *fptr, /* I - FITS file pointer                        */
            int bitpix,     /* I - number of bits per data value pixel      */
            int naxis,      /* I - number of axes in the data array         */
            long naxes[],   /* I - length of each data axis                 */
            int *status)    /* IO - error status                            */
/*
  write STANDARD set of required primary header keywords
*/
{
    int simple = 1;     /* does file conform to FITS standard? 1/0  */
    long pcount = 0;    /* number of group parameters (usually 0)   */
    long gcount = 1;    /* number of random groups (usually 1 or 0) */
    int extend = 1;     /* may FITS file have extensions?           */

    ffphpr(fptr, simple, bitpix, naxis, naxes, pcount, gcount, extend, status);
    return(*status);
}

/*--------------------------------------------------------------------------*/
int ffphpr( fitsfile *fptr, /* I - FITS file pointer                        */
            int simple,     /* I - does file conform to FITS standard? 1/0  */
            int bitpix,     /* I - number of bits per data value pixel      */
            int naxis,      /* I - number of axes in the data array         */
            long naxes[],   /* I - length of each data axis                 */
            long pcount,    /* I - number of group parameters (usually 0)   */
            long gcount,    /* I - number of random groups (usually 1 or 0) */
            int extend,     /* I - may FITS file have extensions?           */
            int *status)    /* IO - error status                            */
/*
  write required primary header keywords
*/
{
    int ii;
    long longbitpix;
    char name[FLEN_KEYWORD], comm[FLEN_COMMENT], message[FLEN_ERRMSG];

    if (*status > 0)
        return(*status);

    if (fptr->headend != fptr->headstart[fptr->curhdu] )
        return(*status = HEADER_NOT_EMPTY);

    if (fptr->curhdu == 0)
    {                /* write primary array header */
        if (simple)
            strcpy(comm, "file does conform to FITS standard");
        else
            strcpy(comm, "file does not conform to FITS standard");

        ffpkyl(fptr, "SIMPLE", simple, comm, status);
    }
    else
    {               /* write IMAGE extension header */
        strcpy(comm, "IMAGE extension");
        ffpkys(fptr, "XTENSION", "IMAGE", comm, status);
    }

    longbitpix = bitpix;
    if (longbitpix != 8 && longbitpix != 16 && longbitpix != 32 &&
             longbitpix != -32 && longbitpix != -64)
    {
        sprintf(message,
        "Illegal value for BITPIX keyword: %d", bitpix);
        ffpmsg(message);
        return(*status = BAD_BITPIX);
    }

    strcpy(comm, "number of bits per data pixel");
    if (ffpkyj(fptr, "BITPIX", longbitpix, comm, status) > 0)
        return(*status);


    if (naxis < 0 || naxis > 999)
    {
        sprintf(message,
        "Illegal value for NAXIS keyword: %d", naxis);
        ffpmsg(message);
        return(*status = BAD_NAXIS);
    }

    strcpy(comm, "number of data axes");
    ffpkyj(fptr, "NAXIS", naxis, comm, status);

    strcpy(comm, "length of data axis ");
    for (ii = 0; ii < naxis; ii++)
    {
        if (naxes[ii] < 0)
        {
            sprintf(message,
            "Illegal value for NAXIS%d keyword: %ld", ii + 1,  naxes[ii]);
            ffpmsg(message);
            return(*status = BAD_NAXES);
        }

        sprintf(&comm[20], "%d", ii + 1);
        ffkeyn("NAXIS", ii + 1, name, status);
        ffpkyj(fptr, name, naxes[ii], comm, status);
    }

    if (fptr->curhdu == 0)  /* the primary array */
    {
        if (extend)
        {
            /* only write EXTEND keyword if value = true */
            strcpy(comm, "FITS dataset may contain extensions");
            ffpkyl(fptr, "EXTEND", extend, comm, status);
        }

        if (pcount < 0)
            return(*status = BAD_PCOUNT);

        else if (gcount < 1)
            return(*status = BAD_GCOUNT);

        else if (pcount > 0 || gcount > 1)
        {
            /* only write these keyword if non-standard values */
            strcpy(comm, "random group records are present");
            ffpkyl(fptr, "GROUPS", 1, comm, status);

            strcpy(comm, "number of random group parameters");
            ffpkyj(fptr, "PCOUNT", pcount, comm, status);
  
            strcpy(comm, "number of random groups");
            ffpkyj(fptr, "GCOUNT", gcount, comm, status);
        }

      /* write standard block of self-documentating comments */
      ffpcom(fptr,
      "FITS (Flexible Image Transport System) format defined in Astronomy and",
      status);

      ffpcom(fptr,
      "Astrophysics Supplement Series v44/p363, v44/p371, v73/p359, v73/p365.",
      status);

      ffpcom(fptr,
      "Contact the NASA Science Office of Standards and Technology for the",
      status);

      ffpcom(fptr,
      "FITS Definition document #100 and other FITS information.", status);
    }

    else  /* an IMAGE extension */

    {   /* image extension; cannot have random groups */
        if (pcount != 0)
            *status = BAD_PCOUNT;

        else if (gcount != 1)
            *status = BAD_GCOUNT;

        else
        {
            strcpy(comm, "required keyword; must = 0");
            ffpkyj(fptr, "PCOUNT", pcount, comm, status);
  
            strcpy(comm, "required keyword; must = 1");
            ffpkyj(fptr, "GCOUNT", gcount, comm, status);
        }
    }
    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffphtb(fitsfile *fptr,  /* I - FITS file pointer                        */
           long naxis1,     /* I - width of row in the table                */
           long naxis2,     /* I - number of rows in the table              */
           int tfields,     /* I - number of columns in the table           */
           char **ttype,    /* I - name of each column                      */
           long *tbcol,     /* I - byte offset in row to each column        */
           char **tform,    /* I - value of TFORMn keyword for each column  */
           char **tunit,    /* I - value of TUNITn keyword for each column  */
           char *extname,   /* I - value of EXTNAME keyword, if any         */
           int *status)     /* IO - error status                            */
/*
  Put required Header keywords into the ASCII TaBle:
*/
{
    int ii;
    char tfmt[30], name[FLEN_KEYWORD], comm[FLEN_COMMENT];

    if (*status > 0)
        return(*status);
    else if (fptr->headend != fptr->headstart[fptr->curhdu] )
        return(*status = HEADER_NOT_EMPTY);
    else if (naxis1 < 0)
        return(*status = NEG_WIDTH);
    else if (naxis2 < 0)
        return(*status = NEG_ROWS);
    else if (tfields < 0 || tfields > 999)
        return(*status = BAD_TFIELDS);
    
    ffpkys(fptr, "XTENSION", "TABLE", "ASCII table extension", status);
    ffpkyj(fptr, "BITPIX", 8, "8-bit ASCII characters", status);
    ffpkyj(fptr, "NAXIS", 2, "2-dimensional ASCII table", status);
    ffpkyj(fptr, "NAXIS1", naxis1, "width of table in characters", status);
    ffpkyj(fptr, "NAXIS2", naxis2, "number of rows in table", status);
    ffpkyj(fptr, "PCOUNT", 0, "no group parameters (required keyword)", status);
    ffpkyj(fptr, "GCOUNT", 1, "one data group (required keyword)", status);
    ffpkyj(fptr, "TFIELDS", tfields, "number of fields in each row", status);

    for (ii = 0; ii < tfields; ii++) /* loop over every column */
    {
        if ( *(ttype[ii]) )  /* optional TTYPEn keyword */
        {
          sprintf(comm, "label for field %3d", ii + 1);
          ffkeyn("TTYPE", ii + 1, name, status);
          ffpkys(fptr, name, ttype[ii], comm, status);
        }

        if (tbcol[ii] < 1 || tbcol[ii] > naxis1)
           *status = BAD_TBCOL;

        sprintf(comm, "beginning column of field %3d", ii + 1);
        ffkeyn("TBCOL", ii + 1, name, status);
        ffpkyj(fptr, name, tbcol[ii], comm, status);

        strcpy(tfmt, tform[ii]);  /* required TFORMn keyword */
        ffupch(tfmt);
        ffkeyn("TFORM", ii + 1, name, status);
        ffpkys(fptr, name, tfmt, "Fortran-77 format of field", status);

        if ( *(tunit[ii]) )       /* optional TUNITn keyword */
        {
          ffkeyn("TUNIT", ii + 1, name, status);
          ffpkys(fptr, name, tunit[ii], "physical unit of field", status) ;
        }

        if (*status > 0)
            break;       /* abort loop on error */
    }

    if (extname[0])       /* optional EXTNAME keyword */
        ffpkys(fptr, "EXTNAME", extname,
               "name of this ASCII table extension", status);

    if (*status > 0)
        ffpmsg("Failed to write ASCII table header keywords (ffphtb)");

    return(*status);
}
/*--------------------------------------------------------------------------*/
int ffphbn(fitsfile *fptr,  /* I - FITS file pointer                        */
           long naxis2,     /* I - number of rows in the table              */
           int tfields,     /* I - number of columns in the table           */
           char **ttype,    /* I - name of each column                      */
           char **tform,    /* I - value of TFORMn keyword for each column  */
           char **tunit,    /* I - value of TUNITn keyword for each column  */
           char *extname,   /* I - value of EXTNAME keyword, if any         */
           long pcount,     /* I - size of the variable length heap area    */
           int *status)     /* IO - error status                            */
/*
  Put required Header keywords into the Binary Table:
*/
{
    int ii, datatype;
    long repeat, width, naxis1;

    char tfmt[30], name[FLEN_KEYWORD], comm[FLEN_COMMENT];

    if (*status > 0)
        return(*status);
    else if (fptr->headend != fptr->headstart[fptr->curhdu] )
        return(*status = HEADER_NOT_EMPTY);
    else if (naxis2 < 0)
        return(*status = NEG_ROWS);
    else if (pcount < 0)
        return(*status = BAD_PCOUNT);
    else if (tfields < 0 || tfields > 999)
        return(*status = BAD_TFIELDS);

    ffpkys(fptr, "XTENSION", "BINTABLE", "binary table extension", status);
    ffpkyj(fptr, "BITPIX", 8, "8-bit ASCII characters", status);
    ffpkyj(fptr, "NAXIS", 2, "2-dimensional binary table", status);

    naxis1 = 0;
    for (ii = 0; ii < tfields; ii++)  /* sum the width of each field */
    {
        ffbnfm(tform[ii], &datatype, &repeat, &width, status);

        if (datatype == FTYPE_ASCII)
            naxis1 += repeat;   /* one byte per char */
        else if (datatype == FTYPE_BIT)
            naxis1 += (repeat + 7) / 8;
        else if (datatype > 0)
            naxis1 += repeat * (datatype / 10);
        else   /* this is a variable length descriptor (neg. datatype) */
            naxis1 += 8;

        if (*status > 0)
            break;       /* abort loop on error */
    }

    ffpkyj(fptr, "NAXIS1", naxis1, "width of table in characters", status);
    ffpkyj(fptr, "NAXIS2", naxis2, "number of rows in table", status);
    ffpkyj(fptr, "PCOUNT", pcount, "size of special data area", status);
    ffpkyj(fptr, "GCOUNT", 1, "one data group (required keyword)", status);
    ffpkyj(fptr, "TFIELDS", tfields, "number of fields in each row", status);

    for (ii = 0; ii < tfields; ii++) /* loop over every column */
    {
        if ( *(ttype[ii]) )  /* optional TTYPEn keyword */
        {
          sprintf(comm, "label for field %3d", ii + 1);
          ffkeyn("TTYPE", ii + 1, name, status);
          ffpkys(fptr, name, ttype[ii], comm, status);
        }

        strcpy(tfmt, tform[ii]);  /* required TFORMn keyword */
        ffupch(tfmt);
        ffkeyn("TFORM", ii + 1, name, status);
        strcpy(comm, "data format of field");

        ffbnfm(tfmt, &datatype, &repeat, &width, status);

        if (datatype == FTYPE_ASCII)
            strcat(comm, ": ASCII Character");
        else if (datatype == FTYPE_BIT)
           strcat(comm, ": BIT");
        else if (datatype == FTYPE_BYTE)
           strcat(comm, ": BYTE");
        else if (datatype == FTYPE_LOGICAL)
           strcat(comm, ": 1-byte LOGICAL");
        else if (datatype == FTYPE_SHORT)
           strcat(comm, ": 2-byte INTEGER");
        else if (datatype == FTYPE_LONG)
           strcat(comm, ": 4-byte INTEGER");
        else if (datatype == FTYPE_FLOAT)
           strcat(comm, ": 4-byte REAL");
        else if (datatype == FTYPE_DOUBLE)
           strcat(comm, ": 8-byte DOUBLE");
        else if (datatype == FTYPE_COMPLEX)
           strcat(comm, ": COMPLEX");
        else if (datatype == FTYPE_DBLCOMPLEX)
           strcat(comm, ": DOUBLE COMPLEX");
        else if (datatype < 0)
           strcat(comm, ": variable length array");

        ffpkys(fptr, name, tfmt, comm, status);

        if ( *(tunit[ii]) )       /* optional TUNITn keyword */
        {
          ffkeyn("TUNIT", ii + 1, name, status);
          ffpkys(fptr, name, tunit[ii],
             "physical unit of field", status);
        }

        if (*status > 0)
            break;       /* abort loop on error */
    }

    if (extname[0])       /* optional EXTNAME keyword */
        ffpkys(fptr, "EXTNAME", extname,
               "name of this ASCII table extension", status);

    if (*status > 0)
        ffpmsg("Failed to write binary table header keywords (ffphbn)");

    return(*status);
}
