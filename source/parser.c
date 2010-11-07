/*!
\file parser.c
\brief hand-coded Algol 68 scanner and parser
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*
This is the scanner. The source file is read and stored internally, is
tokenised, and if needed a refinement preprocessor elaborates a stepwise
refined program. The result is a linear list of tokens that is input for the
parser, that will transform the linear list into a syntax tree.

Algol68G tokenises all symbols before the parser is invoked. This means that
scanning does not use information from the parser.

The scanner does of course do some rudimentary parsing. Format texts can have
enclosed clauses in them, so we record information in a stack as to know
what is being scanned. Also, the refinement preprocessor implements a
(trivial) grammar.

The scanner supports two stropping regimes: bold and quote. Examples of both:

       bold stropping: BEGIN INT i = 1, j = 1; print (i + j) END

       quote stropping: 'BEGIN' 'INT' I = 1, J = 1; PRINT (I + J) 'END'

Quote stropping was used frequently in the (excusez-le-mot) punch-card age.
Hence, bold stropping is the default. There also existed point stropping, but
that has not been implemented here. */

#include "config.h"
#include "algol68g.h"
#include "environ.h"
#include "interpreter.h"

#define STOP_CHAR 127

#define IN_PRELUDE(p) (LINE_NUMBER (p) <= 0)
#define EOL(c) ((c) == NEWLINE_CHAR || (c) == NULL_CHAR)

void file_format_error (int);
static void append_source_line (char *, SOURCE_LINE_T **, int *, char *);

static char *scan_buf;
static int max_scan_buf_length, source_file_size;
static BOOL_T stop_scanner = A68_FALSE, read_error = A68_FALSE, no_preprocessing = A68_FALSE;

/*!
\brief add keyword to the tree
\param p top keyword
\param a attribute
\param t keyword text
**/

static void add_keyword (KEYWORD_T ** p, int a, char *t)
{
  while (*p != NULL) {
    int k = strcmp (t, TEXT (*p));
    if (k < 0) {
      p = &LESS (*p);
    } else {
      p = &MORE (*p);
    }
  }
  *p = (KEYWORD_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (KEYWORD_T));
  ATTRIBUTE (*p) = a;
  TEXT (*p) = t;
  LESS (*p) = MORE (*p) = NULL;
}

/*!
\brief make tables of keywords and non-terminals
**/

void set_up_tables (void)
{
/* Entries are randomised to balance the tree. */
  if (program.options.strict == A68_FALSE) {
    add_keyword (&top_keyword, ENVIRON_SYMBOL, "ENVIRON");
    add_keyword (&top_keyword, DOWNTO_SYMBOL, "DOWNTO");
    add_keyword (&top_keyword, UNTIL_SYMBOL, "UNTIL");
    add_keyword (&top_keyword, CLASS_SYMBOL, "CLASS");
    add_keyword (&top_keyword, NEW_SYMBOL, "NEW");
    add_keyword (&top_keyword, DIAGONAL_SYMBOL, "DIAG");
    add_keyword (&top_keyword, TRANSPOSE_SYMBOL, "TRNSP");
    add_keyword (&top_keyword, ROW_SYMBOL, "ROW");
    add_keyword (&top_keyword, COLUMN_SYMBOL, "COL");
    add_keyword (&top_keyword, ROW_ASSIGN_SYMBOL, "::=");
    add_keyword (&top_keyword, CODE_SYMBOL, "CODE");
    add_keyword (&top_keyword, EDOC_SYMBOL, "EDOC");
    add_keyword (&top_keyword, ANDF_SYMBOL, "THEF");
    add_keyword (&top_keyword, ORF_SYMBOL, "ELSF");
    add_keyword (&top_keyword, ANDF_SYMBOL, "ANDTH");
    add_keyword (&top_keyword, ORF_SYMBOL, "OREL");
    add_keyword (&top_keyword, ANDF_SYMBOL, "ANDF");
    add_keyword (&top_keyword, ORF_SYMBOL, "ORF");
  }
  add_keyword (&top_keyword, POINT_SYMBOL, ".");
  add_keyword (&top_keyword, COMPLEX_SYMBOL, "COMPLEX");
  add_keyword (&top_keyword, ACCO_SYMBOL, "{");
  add_keyword (&top_keyword, OCCA_SYMBOL, "}");
  add_keyword (&top_keyword, SOUND_SYMBOL, "SOUND");
  add_keyword (&top_keyword, COLON_SYMBOL, ":");
  add_keyword (&top_keyword, THEN_BAR_SYMBOL, "|");
  add_keyword (&top_keyword, SUB_SYMBOL, "[");
  add_keyword (&top_keyword, BY_SYMBOL, "BY");
  add_keyword (&top_keyword, OP_SYMBOL, "OP");
  add_keyword (&top_keyword, COMMA_SYMBOL, ",");
  add_keyword (&top_keyword, AT_SYMBOL, "AT");
  add_keyword (&top_keyword, PRIO_SYMBOL, "PRIO");
  add_keyword (&top_keyword, STYLE_I_COMMENT_SYMBOL, "CO");
  add_keyword (&top_keyword, END_SYMBOL, "END");
  add_keyword (&top_keyword, GO_SYMBOL, "GO");
  add_keyword (&top_keyword, TO_SYMBOL, "TO");
  add_keyword (&top_keyword, ELSE_BAR_SYMBOL, "|:");
  add_keyword (&top_keyword, THEN_SYMBOL, "THEN");
  add_keyword (&top_keyword, TRUE_SYMBOL, "TRUE");
  add_keyword (&top_keyword, PROC_SYMBOL, "PROC");
  add_keyword (&top_keyword, FOR_SYMBOL, "FOR");
  add_keyword (&top_keyword, GOTO_SYMBOL, "GOTO");
  add_keyword (&top_keyword, WHILE_SYMBOL, "WHILE");
  add_keyword (&top_keyword, IS_SYMBOL, ":=:");
  add_keyword (&top_keyword, ASSIGN_TO_SYMBOL, "=:");
  add_keyword (&top_keyword, COMPL_SYMBOL, "COMPL");
  add_keyword (&top_keyword, FROM_SYMBOL, "FROM");
  add_keyword (&top_keyword, BOLD_PRAGMAT_SYMBOL, "PRAGMAT");
  add_keyword (&top_keyword, BOLD_COMMENT_SYMBOL, "COMMENT");
  add_keyword (&top_keyword, DO_SYMBOL, "DO");
  add_keyword (&top_keyword, STYLE_II_COMMENT_SYMBOL, "#");
  add_keyword (&top_keyword, CASE_SYMBOL, "CASE");
  add_keyword (&top_keyword, LOC_SYMBOL, "LOC");
  add_keyword (&top_keyword, CHAR_SYMBOL, "CHAR");
  add_keyword (&top_keyword, ISNT_SYMBOL, ":/=:");
  add_keyword (&top_keyword, REF_SYMBOL, "REF");
  add_keyword (&top_keyword, NIL_SYMBOL, "NIL");
  add_keyword (&top_keyword, ASSIGN_SYMBOL, ":=");
  add_keyword (&top_keyword, FI_SYMBOL, "FI");
  add_keyword (&top_keyword, FILE_SYMBOL, "FILE");
  add_keyword (&top_keyword, PAR_SYMBOL, "PAR");
  add_keyword (&top_keyword, ASSERT_SYMBOL, "ASSERT");
  add_keyword (&top_keyword, OUSE_SYMBOL, "OUSE");
  add_keyword (&top_keyword, IN_SYMBOL, "IN");
  add_keyword (&top_keyword, LONG_SYMBOL, "LONG");
  add_keyword (&top_keyword, SEMI_SYMBOL, ";");
  add_keyword (&top_keyword, EMPTY_SYMBOL, "EMPTY");
  add_keyword (&top_keyword, MODE_SYMBOL, "MODE");
  add_keyword (&top_keyword, IF_SYMBOL, "IF");
  add_keyword (&top_keyword, OD_SYMBOL, "OD");
  add_keyword (&top_keyword, OF_SYMBOL, "OF");
  add_keyword (&top_keyword, STRUCT_SYMBOL, "STRUCT");
  add_keyword (&top_keyword, STYLE_I_PRAGMAT_SYMBOL, "PR");
  add_keyword (&top_keyword, BUS_SYMBOL, "]");
  add_keyword (&top_keyword, SKIP_SYMBOL, "SKIP");
  add_keyword (&top_keyword, SHORT_SYMBOL, "SHORT");
  add_keyword (&top_keyword, IS_SYMBOL, "IS");
  add_keyword (&top_keyword, ESAC_SYMBOL, "ESAC");
  add_keyword (&top_keyword, CHANNEL_SYMBOL, "CHANNEL");
  add_keyword (&top_keyword, REAL_SYMBOL, "REAL");
  add_keyword (&top_keyword, STRING_SYMBOL, "STRING");
  add_keyword (&top_keyword, BOOL_SYMBOL, "BOOL");
  add_keyword (&top_keyword, ISNT_SYMBOL, "ISNT");
  add_keyword (&top_keyword, FALSE_SYMBOL, "FALSE");
  add_keyword (&top_keyword, UNION_SYMBOL, "UNION");
  add_keyword (&top_keyword, OUT_SYMBOL, "OUT");
  add_keyword (&top_keyword, OPEN_SYMBOL, "(");
  add_keyword (&top_keyword, BEGIN_SYMBOL, "BEGIN");
  add_keyword (&top_keyword, FLEX_SYMBOL, "FLEX");
  add_keyword (&top_keyword, VOID_SYMBOL, "VOID");
  add_keyword (&top_keyword, BITS_SYMBOL, "BITS");
  add_keyword (&top_keyword, ELSE_SYMBOL, "ELSE");
  add_keyword (&top_keyword, EXIT_SYMBOL, "EXIT");
  add_keyword (&top_keyword, HEAP_SYMBOL, "HEAP");
  add_keyword (&top_keyword, INT_SYMBOL, "INT");
  add_keyword (&top_keyword, BYTES_SYMBOL, "BYTES");
  add_keyword (&top_keyword, PIPE_SYMBOL, "PIPE");
  add_keyword (&top_keyword, FORMAT_SYMBOL, "FORMAT");
  add_keyword (&top_keyword, SEMA_SYMBOL, "SEMA");
  add_keyword (&top_keyword, CLOSE_SYMBOL, ")");
  add_keyword (&top_keyword, AT_SYMBOL, "@");
  add_keyword (&top_keyword, ELIF_SYMBOL, "ELIF");
  add_keyword (&top_keyword, FORMAT_DELIMITER_SYMBOL, "$");
}

/*!
\brief save scanner state, for character look-ahead
\param ref_l source line
\param ref_s position in source line text
\param ch last scanned character
**/

static void save_state (SOURCE_LINE_T * ref_l, char *ref_s, char ch)
{
  program.scan_state.save_l = ref_l;
  program.scan_state.save_s = ref_s;
  program.scan_state.save_c = ch;
}

/*!
\brief restore scanner state, for character look-ahead
\param ref_l source line
\param ref_s position in source line text
\param ch last scanned character
**/

static void restore_state (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  *ref_l = program.scan_state.save_l;
  *ref_s = program.scan_state.save_s;
  *ch = program.scan_state.save_c;
}

/*!
\brief whether ch is unworthy
\param u source line with error
\param v character in line
\param ch
**/

static void unworthy (SOURCE_LINE_T * u, char *v, char ch)
{
  if (IS_PRINT (ch)) {
    ASSERT (snprintf (edit_line, BUFFER_SIZE, "%s", ERROR_UNWORTHY_CHARACTER) >= 0);
  } else {
    ASSERT (snprintf (edit_line, BUFFER_SIZE, "%s %s", ERROR_UNWORTHY_CHARACTER, ctrl_char (ch)) >= 0);
  }
  scan_error (u, v, edit_line);
}

/*!
\brief concatenate lines that terminate in '\' with next line
\param top top source line
**/

static void concatenate_lines (SOURCE_LINE_T * top)
{
  SOURCE_LINE_T *q;
/* Work from bottom backwards. */
  for (q = top; q != NULL && NEXT (q) != NULL; q = NEXT (q)) {
    ;
  }
  for (; q != NULL; q = PREVIOUS (q)) {
    char *z = q->string;
    int len = (int) strlen (z);
    if (len >= 2 && z[len - 2] == ESCAPE_CHAR && z[len - 1] == NEWLINE_CHAR && NEXT (q) != NULL && NEXT (q)->string != NULL) {
      z[len - 2] = NULL_CHAR;
      len += (int) strlen (NEXT (q)->string);
      z = (char *) get_fixed_heap_space ((size_t) (len + 1));
      bufcpy (z, q->string, len + 1);
      bufcat (z, NEXT (q)->string, len + 1);
      NEXT (q)->string[0] = NULL_CHAR;
      q->string = z;
    }
  }
}

/*!
\brief whether u is bold tag v, independent of stropping regime
\param z source line
\param u symbol under test
\param v bold symbol 
\return whether u is v
**/

static BOOL_T whether_bold (char *u, char *v)
{
  unsigned len = (unsigned) strlen (v);
  if (program.options.stropping == QUOTE_STROPPING) {
    if (u[0] == '\'') {
      return ((BOOL_T) (strncmp (++u, v, len) == 0 && u[len] == '\''));
    } else {
      return (A68_FALSE);
    }
  } else {
    return ((BOOL_T) (strncmp (u, v, len) == 0 && !IS_UPPER (u[len])));
  }
}

/*!
\brief skip string
\param top current source line
\param ch current character in source line
\return whether string is properly terminated
**/

static BOOL_T skip_string (SOURCE_LINE_T ** top, char **ch)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (v[0] == QUOTE_CHAR && v[1] != QUOTE_CHAR) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (v[0] == QUOTE_CHAR && v[1] == QUOTE_CHAR) {
        v += 2;
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (A68_FALSE);
}

/*!
\brief skip comment
\param top current source line
\param ch current character in source line
\param delim expected terminating delimiter
\return whether comment is properly terminated
**/

static BOOL_T skip_comment (SOURCE_LINE_T ** top, char **ch, int delim)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  v++;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (whether_bold (v, "COMMENT") && delim == BOLD_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (whether_bold (v, "CO") && delim == STYLE_I_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (v[0] == '#' && delim == STYLE_II_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (A68_FALSE);
}

/*!
\brief skip rest of pragmat
\param top current source line
\param ch current character in source line
\param delim expected terminating delimiter
\param whitespace whether other pragmat items are allowed
\return whether pragmat is properly terminated
**/

static BOOL_T skip_pragmat (SOURCE_LINE_T ** top, char **ch, int delim, BOOL_T whitespace)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (whether_bold (v, "PRAGMAT") && delim == BOLD_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (whether_bold (v, "PR")
                 && delim == STYLE_I_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else {
        if (whitespace && !IS_SPACE (v[0]) && v[0] != NEWLINE_CHAR) {
          scan_error (u, v, ERROR_PRAGMENT);
        } else if (IS_UPPER (v[0])) {
/* Skip a bold word as you may trigger on REPR, for instance ... */
          while (IS_UPPER (v[0])) {
            v++;
          }
        } else {
          v++;
        }
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (A68_FALSE);
}

/*!
\brief return pointer to next token within pragmat
\param top current source line
\param ch current character in source line
\return pointer to next item, NULL if none remains
**/

static char *get_pragmat_item (SOURCE_LINE_T ** top, char **ch)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      if (!IS_SPACE (v[0]) && v[0] != NEWLINE_CHAR) {
        *top = u;
        *ch = v;
        return (v);
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  return (NULL);
}

/*!
\brief case insensitive strncmp for at most the number of chars in 'v'
\param u string 1, must not be NULL
\param v string 2, must not be NULL
\return alphabetic difference between 1 and 2
**/

static int streq (char *u, char *v)
{
  int diff;
  for (diff = 0; diff == 0 && u[0] != NULL_CHAR && v[0] != NULL_CHAR; u++, v++) {
    diff = ((int) TO_LOWER (u[0])) - ((int) TO_LOWER (v[0]));
  }
  return (diff);
}

/*!
\brief scan for next pragmat and yield first pragmat item
\param top current source line
\param ch current character in source line
\param delim expected terminating delimiter
\return pointer to next item or NULL if none remain
**/

static char *next_preprocessor_item (SOURCE_LINE_T ** top, char **ch, int *delim)
{
  SOURCE_LINE_T *u = *top;
  char *v = *ch;
  *delim = 0;
  while (u != NULL) {
    while (v[0] != NULL_CHAR) {
      SOURCE_LINE_T *start_l = u;
      char *start_c = v;
/* STRINGs must be skipped. */
      if (v[0] == QUOTE_CHAR) {
        SCAN_ERROR (!skip_string (&u, &v), start_l, start_c, ERROR_UNTERMINATED_STRING);
      }
/* COMMENTS must be skipped. */
      else if (whether_bold (v, "COMMENT")) {
        SCAN_ERROR (!skip_comment (&u, &v, BOLD_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (whether_bold (v, "CO")) {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_I_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (v[0] == '#') {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_II_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (whether_bold (v, "PRAGMAT") || whether_bold (v, "PR")) {
/* We caught a PRAGMAT. */
        char *item;
        if (whether_bold (v, "PRAGMAT")) {
          *delim = BOLD_PRAGMAT_SYMBOL;
          v = &v[strlen ("PRAGMAT")];
        } else if (whether_bold (v, "PR")) {
          *delim = STYLE_I_PRAGMAT_SYMBOL;
          v = &v[strlen ("PR")];
        }
        item = get_pragmat_item (&u, &v);
        SCAN_ERROR (item == NULL, start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
/* Item "preprocessor" restarts preprocessing if it is off. */
        if (no_preprocessing && streq (item, "PREPROCESSOR") == 0) {
          no_preprocessing = A68_FALSE;
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
/* If preprocessing is switched off, we idle to closing bracket. */
        else if (no_preprocessing) {
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_FALSE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
/* Item "nopreprocessor" stops preprocessing if it is on. */
        if (streq (item, "NOPREPROCESSOR") == 0) {
          no_preprocessing = A68_TRUE;
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
/* Item "INCLUDE" includes a file. */
        else if (streq (item, "INCLUDE") == 0) {
          *top = u;
          *ch = v;
          return (item);
        }
/* Item "READ" includes a file. */
        else if (streq (item, "READ") == 0) {
          *top = u;
          *ch = v;
          return (item);
        }
/* Unrecognised item - probably options handled later by the tokeniser. */
        else {
          SCAN_ERROR (!skip_pragmat (&u, &v, *delim, A68_FALSE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
        }
      } else if (IS_UPPER (v[0])) {
/* Skip a bold word as you may trigger on REPR, for instance ... */
        while (IS_UPPER (v[0])) {
          v++;
        }
      } else {
        v++;
      }
    }
    FORWARD (u);
    if (u != NULL) {
      v = &(u->string[0]);
    } else {
      v = NULL;
    }
  }
  *top = u;
  *ch = v;
  return (NULL);
}

/*!
\brief include files
\param top top source line
**/

static void include_files (SOURCE_LINE_T * top)
{
/*
include_files

syntax: PR read "filename" PR
        PR include "filename" PR

The file gets inserted before the line containing the pragmat. In this way
correct line numbers are preserved which helps diagnostics. A file that has
been included will not be included a second time - it will be ignored. 
*/
  BOOL_T make_pass = A68_TRUE;
  while (make_pass) {
    SOURCE_LINE_T *s, *t, *u = top;
    char *v = &(u->string[0]);
    make_pass = A68_FALSE;
    RESET_ERRNO;
    while (u != NULL) {
      int pr_lim;
      char *item = next_preprocessor_item (&u, &v, &pr_lim);
      SOURCE_LINE_T *start_l = u;
      char *start_c = v;
/* Search for PR include "filename" PR. */
      if (item != NULL && (streq (item, "INCLUDE") == 0 || streq (item, "READ") == 0)) {
        FILE_T fd;
        int n, linum, fsize, k, bytes_read, fnwid;
        char *fbuf, delim;
        char fnb[BUFFER_SIZE], *fn;
/* Skip to filename. */
        if (streq (item, "INCLUDE") == 0) {
          v = &v[strlen ("INCLUDE")];
        } else {
          v = &v[strlen ("READ")];
        }
        while (IS_SPACE (v[0])) {
          v++;
        }
/* Scan quoted filename. */
        SCAN_ERROR ((v[0] != QUOTE_CHAR && v[0] != '\''), start_l, start_c, ERROR_INCORRECT_FILENAME);
        delim = (v++)[0];
        n = 0;
        fnb[0] = NULL_CHAR;
/* Scan Algol 68 string (note: "" denotes a ", while in C it concatenates).*/
        do {
          SCAN_ERROR (EOL (v[0]), start_l, start_c, ERROR_INCORRECT_FILENAME);
          SCAN_ERROR (n == BUFFER_SIZE - 1, start_l, start_c, ERROR_INCORRECT_FILENAME);
          if (v[0] == delim) {
            while (v[0] == delim && v[1] == delim) {
              SCAN_ERROR (n == BUFFER_SIZE - 1, start_l, start_c, ERROR_INCORRECT_FILENAME);
              fnb[n++] = delim;
              fnb[n] = NULL_CHAR;
              v += 2;
            }
          } else if (IS_PRINT (v[0])) {
            fnb[n++] = *(v++);
            fnb[n] = NULL_CHAR;
          } else {
            SCAN_ERROR (A68_TRUE, start_l, start_c, ERROR_INCORRECT_FILENAME);
          }
        } while (v[0] != delim);
/* Insist that the pragmat is closed properly. */
        v = &v[1];
        SCAN_ERROR (!skip_pragmat (&u, &v, pr_lim, A68_TRUE), start_l, start_c, ERROR_UNTERMINATED_PRAGMAT);
/* Filename valid? */
        SCAN_ERROR (n == 0, start_l, start_c, ERROR_INCORRECT_FILENAME);
        fnwid = (int) strlen (program.files.path) + (int) strlen (fnb) + 1;
        fn = (char *) get_fixed_heap_space ((size_t) fnwid);
        bufcpy (fn, program.files.path, fnwid);
        bufcat (fn, fnb, fnwid);
/* Recursive include? Then *ignore* the file. */
        for (t = top; t != NULL; t = NEXT (t)) {
          if (strcmp (t->filename, fn) == 0) {
            goto search_next_pragmat;   /* Eeek! */
          }
        }
/* Access the file. */
        RESET_ERRNO;
        fd = open (fn, O_RDONLY | O_BINARY);
        SCAN_ERROR (fd == -1, start_l, start_c, ERROR_SOURCE_FILE_OPEN);
/* Access the file. */
        RESET_ERRNO;
        fsize = (int) lseek (fd, 0, SEEK_END);
        ASSERT (fsize >= 0);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        fbuf = (char *) get_temp_heap_space ((unsigned) (8 + fsize));
        RESET_ERRNO;
        ASSERT (lseek (fd, 0, SEEK_SET) >= 0);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        RESET_ERRNO;
        bytes_read = (int) io_read (fd, fbuf, (size_t) fsize);
        SCAN_ERROR (errno != 0 || bytes_read != fsize, start_l, start_c, ERROR_FILE_READ);
/* Buffer still usable? */
        if (fsize > max_scan_buf_length) {
          max_scan_buf_length = fsize;
          scan_buf = (char *)
            get_temp_heap_space ((unsigned)
                                 (8 + max_scan_buf_length));
        }
/* Link all lines into the list. */
        linum = 1;
        s = u;
        t = PREVIOUS (u);
        k = 0;
        while (k < fsize) {
          n = 0;
          scan_buf[0] = NULL_CHAR;
          while (k < fsize && fbuf[k] != NEWLINE_CHAR) {
            SCAN_ERROR ((IS_CNTRL (fbuf[k]) && !IS_SPACE (fbuf[k])) || fbuf[k] == STOP_CHAR, start_l, start_c, ERROR_FILE_INCLUDE_CTRL);
            scan_buf[n++] = fbuf[k++];
            scan_buf[n] = NULL_CHAR;
          }
          scan_buf[n++] = NEWLINE_CHAR;
          scan_buf[n] = NULL_CHAR;
          if (k < fsize) {
            k++;
          }
          append_source_line (scan_buf, &t, &linum, fn);
        }
/* Conclude and go find another include directive, if any. */
        NEXT (t) = s;
        PREVIOUS (s) = t;
        concatenate_lines (top);
        ASSERT (close (fd) == 0);
        make_pass = A68_TRUE;
      }
    search_next_pragmat:       /* skip. */ ;
    }
  }
}

/*!
\brief append a source line to the internal source file
\param str text line to be appended
\param ref_l previous source line
\param line_num previous source line number
\param filename name of file being read
**/

static void append_source_line (char *str, SOURCE_LINE_T ** ref_l, int *line_num, char *filename)
{
  SOURCE_LINE_T *z = new_source_line ();
/* Allow shell command in first line, f.i. "#!/usr/share/bin/a68g". */
  if (*line_num == 1) {
    if (strlen (str) >= 2 && strncmp (str, "#!", 2) == 0) {
      ABEND (strstr (str, "run-script") != NULL, ERROR_SHELL_SCRIPT, NULL);
      (*line_num)++;
      return;
    }
  }
  if (program.options.reductions) {
    WRITELN (STDOUT_FILENO, "\"");
    WRITE (STDOUT_FILENO, str);
    WRITE (STDOUT_FILENO, "\"");
  }
/* Link line into the chain. */
  z->string = new_fixed_string (str);
  z->filename = filename;
  NUMBER (z) = (*line_num)++;
  z->print_status = NOT_PRINTED;
  z->list = A68_TRUE;
  z->diagnostics = NULL;
  NEXT (z) = NULL;
  PREVIOUS (z) = *ref_l;
  if (program.top_line == NULL) {
    program.top_line = z;
  }
  if (*ref_l != NULL) {
    NEXT (*ref_l) = z;
  }
  *ref_l = z;
}

/*!
\brief size of source file
\return size of file
**/

static int get_source_size (void)
{
  FILE_T f = program.files.source.fd;
/* This is why WIN32 must open as "read binary". */
  return ((int) lseek (f, 0, SEEK_END));
}

/*!
\brief append environment source lines
\param str line to append
\param ref_l source line after which to append
\param line_num number of source line 'ref_l'
\param name either "prelude" or "postlude"
**/

static void append_environ (char *str, SOURCE_LINE_T ** ref_l, int *line_num, char *name)
{
  char *text = new_string (str);
  while (text != NULL && text[0] != NULL_CHAR) {
    char *car = text;
    char *cdr = a68g_strchr (text, '!');
    int zero_line_num = 0;
    cdr[0] = NULL_CHAR;
    text = &cdr[1];
    (*line_num)++;
    ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, "%s\n", car) >= 0);
    append_source_line (edit_line, ref_l, &zero_line_num, name);
  }
}

/*!
\brief read script file and make internal copy
\return whether reading is satisfactory 
**/

static BOOL_T read_script_file (void)
{
  SOURCE_LINE_T * ref_l = NULL;
  int k, n, num;
  unsigned len;
  BOOL_T file_end = A68_FALSE;
  char filename[BUFFER_SIZE], linenum[BUFFER_SIZE];
  char ch, * fn, * line; 
  char * buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  FILE_T source = program.files.source.fd;
  ABEND (source == -1, "source file not open", NULL);
  buffer[0] = NULL_CHAR;
  n = 0;
  len = (unsigned) (8 + source_file_size);
  buffer = (char *) get_temp_heap_space (len);
  ASSERT (lseek (source, 0, SEEK_SET) >= 0);
  while (!file_end) {
/* Read the original file name */
    filename[0] = NULL_CHAR;
    k = 0;
    if (io_read (source, &ch, 1) == 0) {
      file_end = A68_TRUE;
      continue;
    }
    while (ch != NEWLINE_CHAR) {
      filename[k++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
    }
    filename[k] = NULL_CHAR;
    fn = add_token (&top_token, filename)->text;
/* Read the original file number */
    linenum[0] = NULL_CHAR;
    k = 0;
    ASSERT (io_read (source, &ch, 1) == 1);
    while (ch != NEWLINE_CHAR) {
      linenum[k++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
    }
    linenum[k] = NULL_CHAR;
    num = strtol (linenum, NULL, 10);
    ABEND (errno == ERANGE, "strange line number", NULL);
/* COPY original line into buffer */
    ASSERT (io_read (source, &ch, 1) == 1);
    line = &buffer[n];
    while (ch != NEWLINE_CHAR) {
      buffer[n++] = ch;
      ASSERT (io_read (source, &ch, 1) == 1);
      ABEND ((unsigned) n >= len, "buffer overflow", NULL);
    }
    buffer[n++] = NEWLINE_CHAR;
    buffer[n] = NULL_CHAR;
    append_source_line (line, &ref_l, &num, fn);
  }
  return (A68_TRUE);
}

/*!
\brief read source file and make internal copy
\return whether reading is satisfactory 
**/

static BOOL_T read_source_file (void)
{
  SOURCE_LINE_T *ref_l = NULL;
  int line_num = 0, k, bytes_read;
  ssize_t l;
  FILE_T f = program.files.source.fd;
  char *prelude_start, *postlude, *buffer;
/* Prelude. */
  if (program.options.stropping == UPPER_STROPPING) {
    prelude_start = bold_prelude_start;
    postlude = bold_postlude;
  } else if (program.options.stropping == QUOTE_STROPPING) {
    prelude_start = quote_prelude_start;
    postlude = quote_postlude;
  } else {
    prelude_start = NULL;
    postlude = NULL;
  }
  append_environ (prelude_start, &ref_l, &line_num, "prelude");
/* Read the file into a single buffer, so we save on system calls. */
  line_num = 1;
  buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  RESET_ERRNO;
  ASSERT (lseek (f, 0, SEEK_SET) >= 0);
  ABEND (errno != 0, "error while reading source file", NULL);
  RESET_ERRNO;
  bytes_read = (int) io_read (f, buffer, (size_t) source_file_size);
  ABEND (errno != 0 || bytes_read != source_file_size, "error while reading source file", NULL);
/* Link all lines into the list. */
  k = 0;
  while (k < source_file_size) {
    l = 0;
    scan_buf[0] = NULL_CHAR;
    while (k < source_file_size && buffer[k] != NEWLINE_CHAR) {
      if (k < source_file_size - 1 && buffer[k] == CR_CHAR && buffer[k + 1] == NEWLINE_CHAR) {
        k++;
      } else {
        scan_buf[l++] = buffer[k++];
        scan_buf[l] = NULL_CHAR;
      }
    }
    scan_buf[l++] = NEWLINE_CHAR;
    scan_buf[l] = NULL_CHAR;
    if (k < source_file_size) {
      k++;
    }
    append_source_line (scan_buf, &ref_l, &line_num, program.files.source.name);
    SCAN_ERROR (l != (ssize_t) strlen (scan_buf), NULL, NULL, ERROR_FILE_SOURCE_CTRL);
  }
/* Postlude. */
  append_environ (postlude, &ref_l, &line_num, "postlude");
/* Concatenate lines. */
  concatenate_lines (program.top_line);
/* Include files. */
  include_files (program.top_line);
  return (A68_TRUE);
}

/*!
\brief next_char get next character from internal copy of source file
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param allow_typo whether typographical display features are allowed
\return next char on input
**/

static char next_char (SOURCE_LINE_T ** ref_l, char **ref_s, BOOL_T allow_typo)
{
  char ch;
#if defined NO_TYPO
  allow_typo = A68_FALSE;
#endif
  LOW_STACK_ALERT (NULL);
/* Source empty? */
  if (*ref_l == NULL) {
    return (STOP_CHAR);
  } else {
    (*ref_l)->list = (BOOL_T) (program.options.nodemask & SOURCE_MASK ? A68_TRUE : A68_FALSE);
/* Take new line? */
    if ((*ref_s)[0] == NEWLINE_CHAR || (*ref_s)[0] == NULL_CHAR) {
      *ref_l = NEXT (*ref_l);
      if (*ref_l == NULL) {
        return (STOP_CHAR);
      }
      *ref_s = (*ref_l)->string;
    } else {
      (*ref_s)++;
    }
/* Deliver next char. */
    ch = (*ref_s)[0];
    if (allow_typo && (IS_SPACE (ch) || ch == FORMFEED_CHAR)) {
      ch = next_char (ref_l, ref_s, allow_typo);
    }
    return (ch);
  }
}

/*!
\brief find first character that can start a valid symbol
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
**/

static void get_good_char (char *ref_c, SOURCE_LINE_T ** ref_l, char **ref_s)
{
  while (*ref_c != STOP_CHAR && (IS_SPACE (*ref_c) || (*ref_c == NULL_CHAR))) {
    if (*ref_l != NULL) {
      (*ref_l)->list = (BOOL_T) (program.options.nodemask & SOURCE_MASK ? A68_TRUE : A68_FALSE);
    }
    *ref_c = next_char (ref_l, ref_s, A68_FALSE);
  }
}

/*!
\brief handle a pragment (pragmat or comment)
\param type type of pragment (#, CO, COMMENT, PR, PRAGMAT)
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
**/

static void pragment (int type, SOURCE_LINE_T ** ref_l, char **ref_c)
{
#define INIT_BUFFER {chars_in_buf = 0; scan_buf[chars_in_buf] = NULL_CHAR;}
#define ADD_ONE_CHAR(ch) {scan_buf[chars_in_buf ++] = ch; scan_buf[chars_in_buf] = NULL_CHAR;}
  char c = **ref_c, *term_s = NULL, *start_c = *ref_c;
  SOURCE_LINE_T *start_l = *ref_l;
  int term_s_length, chars_in_buf;
  BOOL_T stop;
/* Set terminator. */
  if (program.options.stropping == UPPER_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "CO";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "COMMENT";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "PR";
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "PRAGMAT";
    }
  } else if (program.options.stropping == QUOTE_STROPPING) {
    if (type == STYLE_I_COMMENT_SYMBOL) {
      term_s = "'CO'";
    } else if (type == STYLE_II_COMMENT_SYMBOL) {
      term_s = "#";
    } else if (type == BOLD_COMMENT_SYMBOL) {
      term_s = "'COMMENT'";
    } else if (type == STYLE_I_PRAGMAT_SYMBOL) {
      term_s = "'PR'";
    } else if (type == BOLD_PRAGMAT_SYMBOL) {
      term_s = "'PRAGMAT'";
    }
  }
  term_s_length = (int) strlen (term_s);
/* Scan for terminator, and process pragmat items. */
  INIT_BUFFER;
  get_good_char (&c, ref_l, ref_c);
  stop = A68_FALSE;
  while (stop == A68_FALSE) {
    SCAN_ERROR (c == STOP_CHAR, start_l, start_c, ERROR_UNTERMINATED_PRAGMENT);
/* A ".." or '..' delimited string in a PRAGMAT. */
    if ((c == QUOTE_CHAR || (c == '\'' && program.options.stropping == UPPER_STROPPING))
        && (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL)) {
      char delim = c;
      BOOL_T eos = A68_FALSE;
      ADD_ONE_CHAR (c);
      c = next_char (ref_l, ref_c, A68_FALSE);
      while (!eos) {
        SCAN_ERROR (EOL (c), start_l, start_c, ERROR_LONG_STRING);
        if (c == delim) {
          ADD_ONE_CHAR (delim);
          c = next_char (ref_l, ref_c, A68_FALSE);
          save_state (*ref_l, *ref_c, c);
          if (c == delim) {
            c = next_char (ref_l, ref_c, A68_FALSE);
          } else {
            restore_state (ref_l, ref_c, &c);
            eos = A68_TRUE;
          }
        } else if (IS_PRINT (c)) {
          ADD_ONE_CHAR (c);
          c = next_char (ref_l, ref_c, A68_FALSE);
        } else {
          unworthy (start_l, start_c, c);
        }
      }
    }
/* On newline we empty the buffer and scan options when appropriate. */
    else if (EOL (c)) {
      if (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL) {
        isolate_options (scan_buf, start_l);
      }
      INIT_BUFFER;
    } else if (IS_PRINT (c)) {
      ADD_ONE_CHAR (c);
    }
    if (chars_in_buf >= term_s_length) {
/* Check whether we encountered the terminator. */
      stop = (BOOL_T) (strcmp (term_s, &(scan_buf[chars_in_buf - term_s_length])) == 0);
    }
    c = next_char (ref_l, ref_c, A68_FALSE);
  }
  scan_buf[chars_in_buf - term_s_length] = NULL_CHAR;
#undef ADD_ONE_CHAR
#undef INIT_BUFFER
}

/*!
\brief attribute for format item
\param ch format item in character form
\return same
**/

static int get_format_item (char ch)
{
  switch (TO_LOWER (ch)) {
    case 'a': { return (FORMAT_ITEM_A); }
    case 'b': { return (FORMAT_ITEM_B); }
    case 'c': { return (FORMAT_ITEM_C); }
    case 'd': { return (FORMAT_ITEM_D); }
    case 'e': { return (FORMAT_ITEM_E); }
    case 'f': { return (FORMAT_ITEM_F); }
    case 'g': { return (FORMAT_ITEM_G); }
    case 'h': { return (FORMAT_ITEM_H); }
    case 'i': { return (FORMAT_ITEM_I); }
    case 'j': { return (FORMAT_ITEM_J); }
    case 'k': { return (FORMAT_ITEM_K); }
    case 'l': case '/': { return (FORMAT_ITEM_L); }
    case 'm': { return (FORMAT_ITEM_M); }
    case 'n': { return (FORMAT_ITEM_N); }
    case 'o': { return (FORMAT_ITEM_O); }
    case 'p': { return (FORMAT_ITEM_P); }
    case 'q': { return (FORMAT_ITEM_Q); }
    case 'r': { return (FORMAT_ITEM_R); }
    case 's': { return (FORMAT_ITEM_S); }
    case 't': { return (FORMAT_ITEM_T); }
    case 'u': { return (FORMAT_ITEM_U); }
    case 'v': { return (FORMAT_ITEM_V); }
    case 'w': { return (FORMAT_ITEM_W); }
    case 'x': { return (FORMAT_ITEM_X); }
    case 'y': { return (FORMAT_ITEM_Y); }
    case 'z': { return (FORMAT_ITEM_Z); }
    case '+': { return (FORMAT_ITEM_PLUS); }
    case '-': { return (FORMAT_ITEM_MINUS); }
    case POINT_CHAR: { return (FORMAT_ITEM_POINT); }
    case '%': { return (FORMAT_ITEM_ESCAPE); }
    default: { return (0); }
  }
}

/* Macros. */

#define SCAN_DIGITS(c)\
  while (IS_DIGIT (c)) {\
    (sym++)[0] = (c);\
    (c) = next_char (ref_l, ref_s, A68_TRUE);\
  }

#define SCAN_EXPONENT_PART(c)\
  (sym++)[0] = EXPONENT_CHAR;\
  (c) = next_char (ref_l, ref_s, A68_TRUE);\
  if ((c) == '+' || (c) == '-') {\
    (sym++)[0] = (c);\
    (c) = next_char (ref_l, ref_s, A68_TRUE);\
  }\
  SCAN_ERROR (!IS_DIGIT (c), *start_l, *start_c, ERROR_EXPONENT_DIGIT);\
  SCAN_DIGITS (c)

/*!
\brief whether input shows exponent character
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param ch last scanned char
\return same
**/

static BOOL_T whether_exp_char (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  char exp_syms[3];
  if (program.options.stropping == UPPER_STROPPING) {
    exp_syms[0] = EXPONENT_CHAR;
    exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[2] = NULL_CHAR;
  } else {
    exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
    exp_syms[1] = ESCAPE_CHAR;
    exp_syms[2] = NULL_CHAR;
  }
  save_state (*ref_l, *ref_s, *ch);
  if (strchr (exp_syms, *ch) != NULL) {
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NULL);
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/*!
\brief whether input shows radix character
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\return same
**/

static BOOL_T whether_radix_char (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (program.options.stropping == QUOTE_STROPPING) {
    if (*ch == TO_UPPER (RADIX_CHAR)) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789ABCDEF", *ch) != NULL);
    }
  } else {
    if (*ch == RADIX_CHAR) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("0123456789abcdef", *ch) != NULL);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/*!
\brief whether input shows decimal point
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\return same
**/

static BOOL_T whether_decimal_point (SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T ret = A68_FALSE;
  save_state (*ref_l, *ref_s, *ch);
  if (*ch == POINT_CHAR) {
    char exp_syms[3];
    if (program.options.stropping == UPPER_STROPPING) {
      exp_syms[0] = EXPONENT_CHAR;
      exp_syms[1] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[2] = NULL_CHAR;
    } else {
      exp_syms[0] = (char) TO_UPPER (EXPONENT_CHAR);
      exp_syms[1] = ESCAPE_CHAR;
      exp_syms[2] = NULL_CHAR;
    }
    *ch = next_char (ref_l, ref_s, A68_TRUE);
    if (strchr (exp_syms, *ch) != NULL) {
      *ch = next_char (ref_l, ref_s, A68_TRUE);
      ret = (BOOL_T) (strchr ("+-0123456789", *ch) != NULL);
    } else {
      ret = (BOOL_T) (strchr ("0123456789", *ch) != NULL);
    }
  }
  restore_state (ref_l, ref_s, ch);
  return (ret);
}

/*!
\brief get next token from internal copy of source file.
\param in_format are we scanning a format text
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param start_l line where token starts
\param start_c character where token starts
\param att attribute designated to token
**/

static void get_next_token (BOOL_T in_format, SOURCE_LINE_T ** ref_l, char **ref_s, SOURCE_LINE_T ** start_l, char **start_c, int *att)
{
  char c = **ref_s, *sym = scan_buf;
  sym[0] = NULL_CHAR;
  get_good_char (&c, ref_l, ref_s);
  *start_l = *ref_l;
  *start_c = *ref_s;
  if (c == STOP_CHAR) {
/* We are at EOF. */
    (sym++)[0] = STOP_CHAR;
    sym[0] = NULL_CHAR;
    return;
  }
/*------------+
| In a format |
+------------*/
  if (in_format) {
    char *format_items;
    if (program.options.stropping == UPPER_STROPPING) {
      format_items = "/%\\+-.abcdefghijklmnopqrstuvwxyz";
    } else {                    /* if (program.options.stropping == QUOTE_STROPPING) */
      format_items = "/%\\+-.ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (a68g_strchr (format_items, c) != NULL) {
/* General format items. */
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      *att = get_format_item (c);
      (void) next_char (ref_l, ref_s, A68_FALSE);
      return;
    }
    if (IS_DIGIT (c)) {
/* INT denotation for static replicator. */
      SCAN_DIGITS (c);
      sym[0] = NULL_CHAR;
      *att = STATIC_REPLICATOR;
      return;
    }
  }
/*----------------+
| Not in a format |
+----------------*/
  if (IS_UPPER (c)) {
    if (program.options.stropping == UPPER_STROPPING) {
/* Upper case word - bold tag. */
      while (IS_UPPER (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      sym[0] = NULL_CHAR;
      *att = BOLD_TAG;
    } else if (program.options.stropping == QUOTE_STROPPING) {
      while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_TRUE);
      }
      sym[0] = NULL_CHAR;
      *att = IDENTIFIER;
    }
  } else if (c == '\'') {
/* Quote, uppercase word, quote - bold tag. */
    int k = 0;
    c = next_char (ref_l, ref_s, A68_FALSE);
    while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      k++;
      c = next_char (ref_l, ref_s, A68_TRUE);
    }
    SCAN_ERROR (k == 0, *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    sym[0] = NULL_CHAR;
    *att = BOLD_TAG;
/* Skip terminating quote, or complain if it is not there. */
    SCAN_ERROR (c != '\'', *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    c = next_char (ref_l, ref_s, A68_FALSE);
  } else if (IS_LOWER (c)) {
/* Lower case word - identifier. */
    while (IS_LOWER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
    }
    sym[0] = NULL_CHAR;
    *att = IDENTIFIER;
  } else if (c == POINT_CHAR) {
/* Begins with a point symbol - point, dotdot, L REAL denotation. */
    if (whether_decimal_point (ref_l, ref_s, &c)) {
      (sym++)[0] = '0';
      (sym++)[0] = POINT_CHAR;
      c = next_char (ref_l, ref_s, A68_TRUE);
      SCAN_DIGITS (c);
      if (whether_exp_char (ref_l, ref_s, &c)) {
        SCAN_EXPONENT_PART (c);
      }
      sym[0] = NULL_CHAR;
      *att = REAL_DENOTATION;
    } else {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (c == POINT_CHAR) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = DOTDOT_SYMBOL;
        c = next_char (ref_l, ref_s, A68_FALSE);
      } else {
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = POINT_SYMBOL;
      }
    }
  } else if (IS_DIGIT (c)) {
/* Something that begins with a digit - L INT denotation, L REAL denotation. */
    SCAN_DIGITS (c);
    if (whether_decimal_point (ref_l, ref_s, &c)) {
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (whether_exp_char (ref_l, ref_s, &c)) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = '0';
        SCAN_EXPONENT_PART (c);
        *att = REAL_DENOTATION;
      } else {
        (sym++)[0] = POINT_CHAR;
        SCAN_DIGITS (c);
        if (whether_exp_char (ref_l, ref_s, &c)) {
          SCAN_EXPONENT_PART (c);
        }
        *att = REAL_DENOTATION;
      }
    } else if (whether_exp_char (ref_l, ref_s, &c)) {
      SCAN_EXPONENT_PART (c);
      *att = REAL_DENOTATION;
    } else if (whether_radix_char (ref_l, ref_s, &c)) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_TRUE);
      if (program.options.stropping == UPPER_STROPPING) {
        while (IS_DIGIT (c) || strchr ("abcdef", c) != NULL) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      } else {
        while (IS_DIGIT (c) || strchr ("ABCDEF", c) != NULL) {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_TRUE);
        }
      }
      *att = BITS_DENOTATION;
    } else {
      *att = INT_DENOTATION;
    }
    sym[0] = NULL_CHAR;
  } else if (c == QUOTE_CHAR) {
/* STRING denotation. */
    BOOL_T stop = A68_FALSE;
    while (!stop) {
      c = next_char (ref_l, ref_s, A68_FALSE);
      while (c != QUOTE_CHAR && c != STOP_CHAR) {
        SCAN_ERROR (EOL (c), *start_l, *start_c, ERROR_LONG_STRING);
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
      SCAN_ERROR (*ref_l == NULL, *start_l, *start_c, ERROR_UNTERMINATED_STRING);
      c = next_char (ref_l, ref_s, A68_FALSE);
      if (c == QUOTE_CHAR) {
        (sym++)[0] = QUOTE_CHAR;
      } else {
        stop = A68_TRUE;
      }
    }
    sym[0] = NULL_CHAR;
    *att = in_format ? LITERAL : ROW_CHAR_DENOTATION;
  } else if (a68g_strchr ("#$()[]{},;@", c) != NULL) {
/* Single character symbols. */
    (sym++)[0] = c;
    (void) next_char (ref_l, ref_s, A68_FALSE);
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '|') {
/* Bar. */
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      (void) next_char (ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '!' && program.options.stropping == QUOTE_STROPPING) {
/* Bar, will be replaced with modern variant.
   For this reason ! is not a MONAD with quote-stropping. */
    (sym++)[0] = '|';
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      (void) next_char (ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == ':') {
/* Colon, semicolon, IS, ISNT. */
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (c == '=') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == ':') {
        (sym++)[0] = c;
        c = next_char (ref_l, ref_s, A68_FALSE);
      }
    } else if (c == '/') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
        if ((c = next_char (ref_l, ref_s, A68_FALSE)) == ':') {
          (sym++)[0] = c;
          c = next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      if ((c = next_char (ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
      }
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '=') {
/* Operator starting with "=". */
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NULL) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (ref_l, ref_s, A68_FALSE);
        if (strlen (sym) < 4 && c == '=') {
          (sym++)[0] = '=';
          (void) next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        (void) next_char (ref_l, ref_s, A68_FALSE);
      } else {
        SCAN_ERROR (!(strcmp (scanned, "=:") == 0 || strcmp (scanned, "==:") == 0), *start_l, *start_c, ERROR_INVALID_OPERATOR_TAG);
      }
    }
    sym[0] = NULL_CHAR;
    if (strcmp (scanned, "=") == 0) {
      *att = EQUALS_SYMBOL;
    } else {
      *att = OPERATOR;
    }
  } else if (a68g_strchr (MONADS, c) != NULL || a68g_strchr (NOMADS, c) != NULL) {
/* Operator. */
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NULL) {
      (sym++)[0] = c;
      c = next_char (ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (ref_l, ref_s, A68_FALSE);
        if (strlen (scanned) < 4 && c == '=') {
          (sym++)[0] = '=';
          (void) next_char (ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        sym[0] = NULL_CHAR;
        (void) next_char (ref_l, ref_s, A68_FALSE);
      } else {
        SCAN_ERROR (strcmp (&(scanned[1]), "=:") != 0, *start_l, *start_c, ERROR_INVALID_OPERATOR_TAG);
      }
    }
    sym[0] = NULL_CHAR;
    *att = OPERATOR;
  } else {
/* Afuuus ... strange characters! */
    unworthy (*start_l, *start_c, (int) c);
  }
}

/*!
\brief whether att opens an embedded clause
\param att attribute under test
\return whether att opens an embedded clause
**/

static BOOL_T open_embedded_clause (int att)
{
  switch (att) {
  case OPEN_SYMBOL:
  case BEGIN_SYMBOL:
  case PAR_SYMBOL:
  case IF_SYMBOL:
  case CASE_SYMBOL:
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case BY_SYMBOL:
  case TO_SYMBOL:
  case DOWNTO_SYMBOL:
  case WHILE_SYMBOL:
  case DO_SYMBOL:
  case SUB_SYMBOL:
  case ACCO_SYMBOL:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether att closes an embedded clause
\param att attribute under test
\return whether att closes an embedded clause
**/

static BOOL_T close_embedded_clause (int att)
{
  switch (att) {
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case FI_SYMBOL:
  case ESAC_SYMBOL:
  case OD_SYMBOL:
  case BUS_SYMBOL:
  case OCCA_SYMBOL:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief cast a string to lower case
\param p string to cast
**/

static void make_lower_case (char *p)
{
  for (; p != NULL && p[0] != NULL_CHAR; p++) {
    p[0] = (char) TO_LOWER (p[0]);
  }
}

/*!
\brief construct a linear list of tokens
\param root node where to insert new symbol
\param level current recursive descent depth
\param in_format whether we scan a format
\param l current source line
\param s current character in source line
\param start_l source line where symbol starts
\param start_c character where symbol starts
**/

static void tokenise_source (NODE_T ** root, int level, BOOL_T in_format, SOURCE_LINE_T ** l, char **s, SOURCE_LINE_T ** start_l, char **start_c)
{
  while (l != NULL && !stop_scanner) {
    int att = 0;
    get_next_token (in_format, l, s, start_l, start_c, &att);
    if (scan_buf[0] == STOP_CHAR) {
      stop_scanner = A68_TRUE;
    } else if (strlen (scan_buf) > 0 || att == ROW_CHAR_DENOTATION || att == LITERAL) {
      KEYWORD_T *kw = find_keyword (top_keyword, scan_buf);
      char *c = NULL;
      BOOL_T make_node = A68_TRUE;
      char *trailing = NULL;
      if (!(kw != NULL && att != ROW_CHAR_DENOTATION)) {
        if (att == IDENTIFIER) {
          make_lower_case (scan_buf);
        }
        if (att != ROW_CHAR_DENOTATION && att != LITERAL) {
          int len = (int) strlen (scan_buf);
          while (len >= 1 && scan_buf[len - 1] == '_') {
            trailing = "_";
            scan_buf[len - 1] = NULL_CHAR;
            len--;
          }
        }
        c = add_token (&top_token, scan_buf)->text;
      } else {
        if (WHETHER (kw, TO_SYMBOL)) {
/* Merge GO and TO to GOTO. */
          if (*root != NULL && WHETHER (*root, GO_SYMBOL)) {
            ATTRIBUTE (*root) = GOTO_SYMBOL;
            SYMBOL (*root) = find_keyword (top_keyword, "GOTO")->text;
            make_node = A68_FALSE;
          } else {
            att = ATTRIBUTE (kw);
            c = kw->text;
          }
        } else {
          if (att == 0 || att == BOLD_TAG) {
            att = ATTRIBUTE (kw);
          }
          c = kw->text;
/* Handle pragments. */
          if (att == STYLE_II_COMMENT_SYMBOL || att == STYLE_I_COMMENT_SYMBOL || att == BOLD_COMMENT_SYMBOL) {
            pragment (ATTRIBUTE (kw), l, s);
            make_node = A68_FALSE;
          } else if (att == STYLE_I_PRAGMAT_SYMBOL || att == BOLD_PRAGMAT_SYMBOL) {
            pragment (ATTRIBUTE (kw), l, s);
            if (!stop_scanner) {
              isolate_options (scan_buf, *start_l);
              (void) set_options (program.options.list, A68_FALSE);
              make_node = A68_FALSE;
            }
          }
        }
      }
/* Add token to the tree. */
      if (make_node) {
        NODE_T *q = new_node ();
        INFO (q) = new_node_info ();
        switch (att) {
          case ASSIGN_SYMBOL:
          case END_SYMBOL:
          case ESAC_SYMBOL:
          case OD_SYMBOL:
          case OF_SYMBOL:
          case FI_SYMBOL:
          case CLOSE_SYMBOL:
          case BUS_SYMBOL:
          case COLON_SYMBOL:
          case COMMA_SYMBOL:
          case DOTDOT_SYMBOL:
          case SEMI_SYMBOL: 
          {
            GENIE (q) = NULL;
            break;
          }
          default: {
            GENIE (q) = new_genie_info ();
            break;
          }
        }
        STATUS (q) = program.options.nodemask;
        LINE (q) = *start_l;
        INFO (q)->char_in_line = *start_c;
        PRIO (INFO (q)) = 0;
        INFO (q)->PROCEDURE_LEVEL = 0;
        ATTRIBUTE (q) = att;
        SYMBOL (q) = c;
        if (program.options.reductions) {
          WRITELN (STDOUT_FILENO, "\"");
          WRITE (STDOUT_FILENO, c);
          WRITE (STDOUT_FILENO, "\"");
        }
        PREVIOUS (q) = *root;
        SUB (q) = NEXT (q) = NULL;
        SYMBOL_TABLE (q) = NULL;
        MOID (q) = NULL;
        TAX (q) = NULL;
        if (*root != NULL) {
          NEXT (*root) = q;
        }
        if (program.top_node == NULL) {
          program.top_node = q;
        }
        *root = q;
        if (trailing != NULL) {
          diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, q, WARNING_TRAILING, trailing, att);
        }
      }
/* 
Redirection in tokenising formats. The scanner is a recursive-descent type as
to know when it scans a format text and when not. 
*/
      if (in_format && att == FORMAT_DELIMITER_SYMBOL) {
        return;
      } else if (!in_format && att == FORMAT_DELIMITER_SYMBOL) {
        tokenise_source (root, level + 1, A68_TRUE, l, s, start_l, start_c);
      } else if (in_format && open_embedded_clause (att)) {
        NODE_T *z = PREVIOUS (*root);
        if (z != NULL && (WHETHER (z, FORMAT_ITEM_N) || WHETHER (z, FORMAT_ITEM_G) || WHETHER (z, FORMAT_ITEM_H) || WHETHER (z, FORMAT_ITEM_F))) {
          tokenise_source (root, level, A68_FALSE, l, s, start_l, start_c);
        } else if (att == OPEN_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (program.options.brackets && att == SUB_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (program.options.brackets && att == ACCO_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        }
      } else if (!in_format && level > 0 && open_embedded_clause (att)) {
        tokenise_source (root, level + 1, A68_FALSE, l, s, start_l, start_c);
      } else if (!in_format && level > 0 && close_embedded_clause (att)) {
        return;
      } else if (in_format && att == CLOSE_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (program.options.brackets && in_format && att == BUS_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (program.options.brackets && in_format && att == OCCA_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      }
    }
  }
}

/*!
\brief tokenise source file, build initial syntax tree
\return whether tokenising ended satisfactorily
**/

BOOL_T lexical_analyser (void)
{
  SOURCE_LINE_T *l, *start_l = NULL;
  char *s = NULL, *start_c = NULL;
  NODE_T *root = NULL;
  scan_buf = NULL;
  max_scan_buf_length = source_file_size = get_source_size ();
/* Errors in file? */
  if (max_scan_buf_length == 0) {
    return (A68_FALSE);
  }
  if (program.options.run_script) {
    scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
    if (!read_script_file ()) {
      return (A68_FALSE);
    }
  } else {
    max_scan_buf_length += (int) strlen (bold_prelude_start) + (int) strlen (bold_postlude);
    max_scan_buf_length += (int) strlen (quote_prelude_start) + (int) strlen (quote_postlude);
/* Allocate a scan buffer with 8 bytes extra space. */
    scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
/* Errors in file? */
    if (!read_source_file ()) {
      return (A68_FALSE);
    }
  }
/* Start tokenising. */
  read_error = A68_FALSE;
  stop_scanner = A68_FALSE;
  if ((l = program.top_line) != NULL) {
    s = l->string;
  }
  tokenise_source (&root, 0, A68_FALSE, &l, &s, &start_l, &start_c);
  return (A68_TRUE);
}

/* This is a small refinement preprocessor for Algol68G. */

/*!
\brief whether refinement terminator
\param p position in syntax tree
\return same
**/

static BOOL_T whether_refinement_terminator (NODE_T * p)
{
  if (WHETHER (p, POINT_SYMBOL)) {
    if (IN_PRELUDE (NEXT (p))) {
      return (A68_TRUE);
    } else {
      return (whether (p, POINT_SYMBOL, IDENTIFIER, COLON_SYMBOL, 0));
    }
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief get refinement definitions in the internal source
\param z module that reads source
**/

void get_refinements (void)
{
  NODE_T *p = program.top_node;
  program.top_refinement = NULL;
/* First look where the prelude ends. */
  while (p != NULL && IN_PRELUDE (p)) {
    FORWARD (p);
  }
/* Determine whether the program contains refinements at all. */
  while (p != NULL && !IN_PRELUDE (p) && !whether_refinement_terminator (p)) {
    FORWARD (p);
  }
  if (p == NULL || IN_PRELUDE (p)) {
    return;
  }
/* Apparently this is code with refinements. */
  FORWARD (p);
  if (p == NULL || IN_PRELUDE (p)) {
/* Ok, we accept a program with no refinements as well. */
    return;
  }
  while (p != NULL && !IN_PRELUDE (p) && whether (p, IDENTIFIER, COLON_SYMBOL, 0)) {
    REFINEMENT_T *new_one = (REFINEMENT_T *) get_fixed_heap_space ((size_t) ALIGNED_SIZE_OF (REFINEMENT_T)), *x;
    BOOL_T exists;
    NEXT (new_one) = NULL;
    new_one->name = SYMBOL (p);
    new_one->applications = 0;
    new_one->line_defined = LINE (p);
    new_one->line_applied = NULL;
    new_one->node_defined = p;
    new_one->begin = NULL;
    new_one->end = NULL;
    p = NEXT_NEXT (p);
    if (p == NULL) {
      diagnostic_node (A68_SYNTAX_ERROR, NULL, ERROR_REFINEMENT_EMPTY, NULL);
      return;
    } else {
      new_one->begin = p;
    }
    while (p != NULL && ATTRIBUTE (p) != POINT_SYMBOL) {
      new_one->end = p;
      FORWARD (p);
    }
    if (p == NULL) {
      diagnostic_node (A68_SYNTAX_ERROR, NULL, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL, NULL);
      return;
    } else {
      FORWARD (p);
    }
/* Do we already have one by this name. */
    x = program.top_refinement;
    exists = A68_FALSE;
    while (x != NULL && !exists) {
      if (x->name == new_one->name) {
        diagnostic_node (A68_SYNTAX_ERROR, new_one->node_defined, ERROR_REFINEMENT_DEFINED, NULL);
        exists = A68_TRUE;
      }
      FORWARD (x);
    }
/* Straight insertion in chain. */
    if (!exists) {
      NEXT (new_one) = program.top_refinement;
      program.top_refinement = new_one;
    }
  }
  if (p != NULL && !IN_PRELUDE (p)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_REFINEMENT_INVALID, NULL);
  }
}

/*!
\brief put refinement applications in the internal source
\param z module that reads source
**/

void put_refinements (void)
{
  REFINEMENT_T *x;
  NODE_T *p, *point;
/* If there are no refinements, there's little to do. */
  if (program.top_refinement == NULL) {
    return;
  }
/* Initialisation. */
  x = program.top_refinement;
  while (x != NULL) {
    x->applications = 0;
    FORWARD (x);
  }
/* Before we introduce infinite loops, find where closing-prelude starts. */
  p = program.top_node;
  while (p != NULL && IN_PRELUDE (p)) {
    FORWARD (p);
  }
  while (p != NULL && !IN_PRELUDE (p)) {
    FORWARD (p);
  }
  ABEND (p == NULL, ERROR_INTERNAL_CONSISTENCY, NULL);
  point = p;
/* We need to substitute until the first point. */
  p = program.top_node;
  while (p != NULL && ATTRIBUTE (p) != POINT_SYMBOL) {
    if (WHETHER (p, IDENTIFIER)) {
/* See if we can find its definition. */
      REFINEMENT_T *y = NULL;
      x = program.top_refinement;
      while (x != NULL && y == NULL) {
        if (x->name == SYMBOL (p)) {
          y = x;
        } else {
          FORWARD (x);
        }
      }
      if (y != NULL) {
/* We found its definition. */
        y->applications++;
        if (y->applications > 1) {
          diagnostic_node (A68_SYNTAX_ERROR, y->node_defined, ERROR_REFINEMENT_APPLIED, NULL);
          FORWARD (p);
        } else {
/* Tie the definition in the tree. */
          y->line_applied = LINE (p);
          if (PREVIOUS (p) != NULL) {
            NEXT (PREVIOUS (p)) = y->begin;
          }
          if (y->begin != NULL) {
            PREVIOUS (y->begin) = PREVIOUS (p);
          }
          if (NEXT (p) != NULL) {
            PREVIOUS (NEXT (p)) = y->end;
          }
          if (y->end != NULL) {
            NEXT (y->end) = NEXT (p);
          }
          p = y->begin;         /* So we can substitute the refinements within. */
        }
      } else {
        FORWARD (p);
      }
    } else {
      FORWARD (p);
    }
  }
/* After the point we ignore it all until the prelude. */
  if (p != NULL && WHETHER (p, POINT_SYMBOL)) {
    if (PREVIOUS (p) != NULL) {
      NEXT (PREVIOUS (p)) = point;
    }
    if (PREVIOUS (point) != NULL) {
      PREVIOUS (point) = PREVIOUS (p);
    }
  } else {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL, NULL);
  }
/* Has the programmer done it well? */
  if (program.error_count == 0) {
    x = program.top_refinement;
    while (x != NULL) {
      if (x->applications == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, x->node_defined, ERROR_REFINEMENT_NOT_APPLIED, NULL);
      }
      FORWARD (x);
    }
  }
}

/*
This file contains a hand-coded parser for Algol 68.

Parsing progresses in various phases to avoid spurious diagnostics from a
recovering parser. Every phase "tightens" the grammar more.
An error in any phase makes the parser quit when that phase ends.
The parser is forgiving in case of superfluous semicolons.

These are the phases:

 (1) Parenthesis are checked to see whether they match.

 (2) Then, a top-down parser determines the basic-block structure of the program
     so symbol tables can be set up that the bottom-up parser will consult
     as you can define things before they are applied.

 (3) A bottom-up parser tries to resolve the structure of the program.

 (4) After the symbol tables have been finalised, a small rearrangement of the
     tree may be required where JUMPs have no GOTO. This leads to the
     non-standard situation that JUMPs without GOTO can have the syntactic
     position of a PRIMARY, SECONDARY or TERTIARY. The mode checker will reject
     such constructs later on.

 (5) The bottom-up parser does not check VICTAL correctness of declarers. This
     is done separately. Also structure of a format text is checked separately.
*/

static jmp_buf bottom_up_crash_exit, top_down_crash_exit;

static int reductions = 0;

static BOOL_T reduce_phrase (NODE_T *, int);
static BOOL_T victal_check_declarer (NODE_T *, int);
static NODE_T *reduce_dyadic (NODE_T *, int u);
static NODE_T *top_down_loop (NODE_T *);
static NODE_T *top_down_skip_unit (NODE_T *);
static void elaborate_bold_tags (NODE_T *);
static void extract_declarations (NODE_T *);
static void extract_identities (NODE_T *);
static void extract_indicants (NODE_T *);
static void extract_labels (NODE_T *, int);
static void extract_operators (NODE_T *);
static void extract_priorities (NODE_T *);
static void extract_proc_identities (NODE_T *);
static void extract_proc_variables (NODE_T *);
static void extract_variables (NODE_T *);
static void try_reduction (NODE_T *, void (*)(NODE_T *), BOOL_T *, ...);
static void ignore_superfluous_semicolons (NODE_T *);
static void recover_from_error (NODE_T *, int, BOOL_T);
static void reduce_arguments (NODE_T *);
static void reduce_basic_declarations (NODE_T *);
static void reduce_bounds (NODE_T *);
static void reduce_collateral_clauses (NODE_T *);
static void reduce_constructs (NODE_T *, int);
static void reduce_control_structure (NODE_T *, int);
static void reduce_declaration_lists (NODE_T *);
static void reduce_declarer_lists (NODE_T *);
static void reduce_declarers (NODE_T *, int);
static void reduce_deeper_clauses (NODE_T *);
static void reduce_deeper_clauses_driver (NODE_T *);
static void reduce_enclosed_clause_bits (NODE_T *, int);
static void reduce_enclosed_clauses (NODE_T *);
static void reduce_enquiry_clauses (NODE_T *);
static void reduce_erroneous_units (NODE_T *);
static void reduce_formal_declarer_pack (NODE_T *);
static void reduce_format_texts (NODE_T *);
static void reduce_formulae (NODE_T *);
static void reduce_generic_arguments (NODE_T *);
static void reduce_indicants (NODE_T *);
static void reduce_labels (NODE_T *);
static void reduce_lengtheties (NODE_T *);
static void reduce_parameter_pack (NODE_T *);
static void reduce_particular_program (NODE_T *);
static void reduce_primaries (NODE_T *, int);
static void reduce_primary_bits (NODE_T *, int);
static void reduce_right_to_left_constructs (NODE_T * q);
static void reduce_row_proc_op_declarers (NODE_T *);
static void reduce_secondaries (NODE_T *);
static void reduce_serial_clauses (NODE_T *);
static void reduce_small_declarers (NODE_T *);
static void reduce_specifiers (NODE_T *);
static void reduce_statements (NODE_T *, int);
static void reduce_struct_pack (NODE_T *);
static void reduce_subordinate (NODE_T *, int);
static void reduce_tertiaries (NODE_T *);
static void reduce_union_pack (NODE_T *);
static void reduce_units (NODE_T *);

/*!
\brief insert node
\param p node after which to insert
\param att attribute for new node
**/

static void insert_node (NODE_T * p, int att)
{
  NODE_T *q = new_node ();
  *q = *p;
  if (GENIE (p) != NULL) {
    GENIE (q) = new_genie_info ();
  }
  ATTRIBUTE (q) = att;
  NEXT (p) = q;
  PREVIOUS (q) = p;
  if (NEXT (q) != NULL) {
    PREVIOUS (NEXT (q)) = q;
  }
}

/*!
\brief substitute brackets
\param p position in tree
**/

void substitute_brackets (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    substitute_brackets (SUB (p));
    switch (ATTRIBUTE (p)) {
    case ACCO_SYMBOL:
      {
        ATTRIBUTE (p) = OPEN_SYMBOL;
        break;
      }
    case OCCA_SYMBOL:
      {
        ATTRIBUTE (p) = CLOSE_SYMBOL;
        break;
      }
    case SUB_SYMBOL:
      {
        ATTRIBUTE (p) = OPEN_SYMBOL;
        break;
      }
    case BUS_SYMBOL:
      {
        ATTRIBUTE (p) = CLOSE_SYMBOL;
        break;
      }
    }
  }
}

/*!
\brief whether token terminates a unit
\param p position in tree
\return TRUE or FALSE whether token terminates a unit
**/

static int whether_unit_terminator (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case BUS_SYMBOL:
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case SEMI_SYMBOL:
  case EXIT_SYMBOL:
  case COMMA_SYMBOL:
  case THEN_BAR_SYMBOL:
  case ELSE_BAR_SYMBOL:
  case THEN_SYMBOL:
  case ELIF_SYMBOL:
  case ELSE_SYMBOL:
  case FI_SYMBOL:
  case IN_SYMBOL:
  case OUT_SYMBOL:
  case OUSE_SYMBOL:
  case ESAC_SYMBOL:
  case EDOC_SYMBOL:
  case OCCA_SYMBOL:
    {
      return (ATTRIBUTE (p));
    }
  default:
    {
      return (NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief whether token is a unit-terminator in a loop clause
\param p position in tree
\return whether token is a unit-terminator in a loop clause
**/

static BOOL_T whether_loop_keyword (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case BY_SYMBOL:
  case TO_SYMBOL:
  case DOWNTO_SYMBOL:
  case WHILE_SYMBOL:
  case DO_SYMBOL:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief whether token cannot follow semicolon or EXIT
\param p position in tree
\return whether token cannot follow semicolon or EXIT
**/

static int whether_semicolon_less (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case BUS_SYMBOL:
  case CLOSE_SYMBOL:
  case END_SYMBOL:
  case SEMI_SYMBOL:
  case EXIT_SYMBOL:
  case THEN_BAR_SYMBOL:
  case ELSE_BAR_SYMBOL:
  case THEN_SYMBOL:
  case ELIF_SYMBOL:
  case ELSE_SYMBOL:
  case FI_SYMBOL:
  case IN_SYMBOL:
  case OUT_SYMBOL:
  case OUSE_SYMBOL:
  case ESAC_SYMBOL:
  case EDOC_SYMBOL:
  case OCCA_SYMBOL:
  case OD_SYMBOL:
  case UNTIL_SYMBOL:
    {
      return (ATTRIBUTE (p));
    }
  default:
    {
      return (NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief get good attribute
\param p position in tree
\return same
**/

static int get_good_attribute (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case UNIT:
  case TERTIARY:
  case SECONDARY:
  case PRIMARY:
    {
      return (get_good_attribute (SUB (p)));
    }
  default:
    {
      return (ATTRIBUTE (p));
    }
  }
}

/*!
\brief preferably don't put intelligible diagnostic here
\param p position in tree
\return same
**/

static BOOL_T dont_mark_here (NODE_T * p)
{
  switch (ATTRIBUTE (p)) {
  case ACCO_SYMBOL:
  case ALT_DO_SYMBOL:
  case ALT_EQUALS_SYMBOL:
  case ANDF_SYMBOL:
  case ASSERT_SYMBOL:
  case ASSIGN_SYMBOL:
  case ASSIGN_TO_SYMBOL:
  case AT_SYMBOL:
  case BEGIN_SYMBOL:
  case BITS_SYMBOL:
  case BOLD_COMMENT_SYMBOL:
  case BOLD_PRAGMAT_SYMBOL:
  case BOOL_SYMBOL:
  case BUS_SYMBOL:
  case BY_SYMBOL:
  case BYTES_SYMBOL:
  case CASE_SYMBOL:
  case CHANNEL_SYMBOL:
  case CHAR_SYMBOL:
  case CLOSE_SYMBOL:
  case CODE_SYMBOL:
  case COLON_SYMBOL:
  case COLUMN_SYMBOL:
  case COMMA_SYMBOL:
  case COMPLEX_SYMBOL:
  case COMPL_SYMBOL:
  case DIAGONAL_SYMBOL:
  case DO_SYMBOL:
  case DOTDOT_SYMBOL:
  case DOWNTO_SYMBOL:
  case EDOC_SYMBOL:
  case ELIF_SYMBOL:
  case ELSE_BAR_SYMBOL:
  case ELSE_SYMBOL:
  case EMPTY_SYMBOL:
  case END_SYMBOL:
  case ENVIRON_SYMBOL:
  case EQUALS_SYMBOL:
  case ESAC_SYMBOL:
  case EXIT_SYMBOL:
  case FALSE_SYMBOL:
  case FILE_SYMBOL:
  case FI_SYMBOL:
  case FLEX_SYMBOL:
  case FORMAT_DELIMITER_SYMBOL:
  case FORMAT_SYMBOL:
  case FOR_SYMBOL:
  case FROM_SYMBOL:
  case GO_SYMBOL:
  case GOTO_SYMBOL:
  case HEAP_SYMBOL:
  case IF_SYMBOL:
  case IN_SYMBOL:
  case INT_SYMBOL:
  case ISNT_SYMBOL:
  case IS_SYMBOL:
  case LOC_SYMBOL:
  case LONG_SYMBOL:
  case MAIN_SYMBOL:
  case MODE_SYMBOL:
  case NIL_SYMBOL:
  case OCCA_SYMBOL:
  case OD_SYMBOL:
  case OF_SYMBOL:
  case OPEN_SYMBOL:
  case OP_SYMBOL:
  case ORF_SYMBOL:
  case OUSE_SYMBOL:
  case OUT_SYMBOL:
  case PAR_SYMBOL:
  case PIPE_SYMBOL:
  case POINT_SYMBOL:
  case PRIO_SYMBOL:
  case PROC_SYMBOL:
  case REAL_SYMBOL:
  case REF_SYMBOL:
  case ROW_ASSIGN_SYMBOL:
  case ROWS_SYMBOL:
  case ROW_SYMBOL:
  case SEMA_SYMBOL:
  case SEMI_SYMBOL:
  case SHORT_SYMBOL:
  case SKIP_SYMBOL:
  case SOUND_SYMBOL:
  case STRING_SYMBOL:
  case STRUCT_SYMBOL:
  case STYLE_I_COMMENT_SYMBOL:
  case STYLE_II_COMMENT_SYMBOL:
  case STYLE_I_PRAGMAT_SYMBOL:
  case SUB_SYMBOL:
  case THEN_BAR_SYMBOL:
  case THEN_SYMBOL:
  case TO_SYMBOL:
  case TRANSPOSE_SYMBOL:
  case TRUE_SYMBOL:
  case UNION_SYMBOL:
  case UNTIL_SYMBOL:
  case VOID_SYMBOL:
  case WHILE_SYMBOL:
/* and than */
  case SERIAL_CLAUSE:
  case ENQUIRY_CLAUSE:
  case INITIALISER_SERIES:
  case DECLARATION_LIST:
    {
      return (A68_TRUE);
    }
  default:
    {
      return (A68_FALSE);
    }
  }
}

/*!
\brief intelligible diagnostic from syntax tree branch
\param p position in tree
\param q
\return same
**/

char *phrase_to_text (NODE_T * p, NODE_T ** w)
{
#define MAX_TERMINALS 8
  int count = 0, line = -1;
  static char buffer[BUFFER_SIZE];
  for (buffer[0] = NULL_CHAR; p != NULL && count < MAX_TERMINALS; FORWARD (p)) {
    if (LINE_NUMBER (p) > 0) {
      int gatt = get_good_attribute (p);
      char *z = non_terminal_string (input_line, gatt);
/* 
Where to put the error message? Bob Uzgalis noted that actual content of a 
diagnostic is not as important as accurately indicating *were* the problem is! 
*/
      if (w != NULL) {
        if (count == 0 || (*w) == NULL) {
          *w = p;
        } else if (dont_mark_here (*w)) {
          *w = p;
        }
      }
/* Add initiation. */
      if (count == 0) {
        if (w != NULL) {
          bufcat (buffer, "construct beginning with", BUFFER_SIZE);
        }
      } else if (count == 1) {
        bufcat (buffer, " followed by", BUFFER_SIZE);
      } else if (count == 2) {
        bufcat (buffer, " and then", BUFFER_SIZE);
      } else if (count >= 3) {
        bufcat (buffer, ",", BUFFER_SIZE);
      }
/* Attribute or symbol. */
      if (z != NULL && SUB (p) != NULL) {
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION) {
          ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
        } else {
          if (strchr ("aeio", z[0]) != NULL) {
            bufcat (buffer, " an", BUFFER_SIZE);
          } else {
            bufcat (buffer, " a", BUFFER_SIZE);
          }
          ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, " %s", z) >= 0);
          bufcat (buffer, edit_line, BUFFER_SIZE);
        }
      } else if (z != NULL && SUB (p) == NULL) {
        ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      } else if (SYMBOL (p) != NULL) {
        ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, " \"%s\"", SYMBOL (p)) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      }
/* Add "starting in line nn". */
      if (z != NULL && line != LINE_NUMBER (p)) {
        line = LINE_NUMBER (p);
        if (gatt == SERIAL_CLAUSE || gatt == ENQUIRY_CLAUSE || gatt == INITIALISER_SERIES) {
          bufcat (buffer, " starting", BUFFER_SIZE);
        }
        ASSERT (snprintf (edit_line, (size_t) BUFFER_SIZE, " in line %d", line) >= 0);
        bufcat (buffer, edit_line, BUFFER_SIZE);
      }
      count++;
    }
  }
  if (p != NULL && count == MAX_TERMINALS) {
    bufcat (buffer, " etcetera", BUFFER_SIZE);
  }
  return (buffer);
}

/*
This is a parenthesis checker. After this checker, we know that at least
brackets are matched. This stabilises later parser phases.
Top-down parsing is done to place error diagnostics near offending lines.
*/

static char bracket_check_error_text[BUFFER_SIZE];

/*!
\brief intelligible diagnostics for the bracket checker
\param txt buffer to which to append text
\param n count mismatch (~= 0)
\param bra opening bracket
\param ket expected closing bracket
\return same
**/

static void bracket_check_error (char *txt, int n, char *bra, char *ket)
{
  if (n != 0) {
    char b[BUFFER_SIZE];
    ASSERT (snprintf (b, (size_t) BUFFER_SIZE, "\"%s\" without matching \"%s\"", (n > 0 ? bra : ket), (n > 0 ? ket : bra)) >= 0);
    if (strlen (txt) > 0) {
      bufcat (txt, " and ", BUFFER_SIZE);
    }
    bufcat (txt, b, BUFFER_SIZE);
  }
}

/*!
\brief diagnose brackets in local branch of the tree
\param p position in tree
\return error message
**/

static char *bracket_check_diagnose (NODE_T * p)
{
  int begins = 0, opens = 0, format_delims = 0, format_opens = 0, subs = 0, ifs = 0, cases = 0, dos = 0, accos = 0;
  for (; p != NULL; FORWARD (p)) {
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        begins++;
        break;
      }
    case END_SYMBOL:
      {
        begins--;
        break;
      }
    case OPEN_SYMBOL:
      {
        opens++;
        break;
      }
    case CLOSE_SYMBOL:
      {
        opens--;
        break;
      }
    case ACCO_SYMBOL:
      {
        accos++;
        break;
      }
    case OCCA_SYMBOL:
      {
        accos--;
        break;
      }
    case FORMAT_DELIMITER_SYMBOL:
      {
        if (format_delims == 0) {
          format_delims = 1;
        } else {
          format_delims = 0;
        }
        break;
      }
    case FORMAT_OPEN_SYMBOL:
      {
        format_opens++;
        break;
      }
    case FORMAT_CLOSE_SYMBOL:
      {
        format_opens--;
        break;
      }
    case SUB_SYMBOL:
      {
        subs++;
        break;
      }
    case BUS_SYMBOL:
      {
        subs--;
        break;
      }
    case IF_SYMBOL:
      {
        ifs++;
        break;
      }
    case FI_SYMBOL:
      {
        ifs--;
        break;
      }
    case CASE_SYMBOL:
      {
        cases++;
        break;
      }
    case ESAC_SYMBOL:
      {
        cases--;
        break;
      }
    case DO_SYMBOL:
      {
        dos++;
        break;
      }
    case OD_SYMBOL:
      {
        dos--;
        break;
      }
    }
  }
  bracket_check_error_text[0] = NULL_CHAR;
  bracket_check_error (bracket_check_error_text, begins, "BEGIN", "END");
  bracket_check_error (bracket_check_error_text, opens, "(", ")");
  bracket_check_error (bracket_check_error_text, format_opens, "(", ")");
  bracket_check_error (bracket_check_error_text, format_delims, "$", "$");
  bracket_check_error (bracket_check_error_text, accos, "{", "}");
  bracket_check_error (bracket_check_error_text, subs, "[", "]");
  bracket_check_error (bracket_check_error_text, ifs, "IF", "FI");
  bracket_check_error (bracket_check_error_text, cases, "CASE", "ESAC");
  bracket_check_error (bracket_check_error_text, dos, "DO", "OD");
  return (bracket_check_error_text);
}

/*!
\brief driver for locally diagnosing non-matching tokens
\param top
\param p position in tree
\return token from where to continue
**/

static NODE_T *bracket_check_parse (NODE_T * top, NODE_T * p)
{
  BOOL_T ignore_token;
  for (; p != NULL; FORWARD (p)) {
    int ket = NULL_ATTRIBUTE;
    NODE_T *q = NULL;
    ignore_token = A68_FALSE;
    switch (ATTRIBUTE (p)) {
    case BEGIN_SYMBOL:
      {
        ket = END_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case OPEN_SYMBOL:
      {
        ket = CLOSE_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case ACCO_SYMBOL:
      {
        ket = OCCA_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case FORMAT_OPEN_SYMBOL:
      {
        ket = FORMAT_CLOSE_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case SUB_SYMBOL:
      {
        ket = BUS_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case IF_SYMBOL:
      {
        ket = FI_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case CASE_SYMBOL:
      {
        ket = ESAC_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case DO_SYMBOL:
      {
        ket = OD_SYMBOL;
        q = bracket_check_parse (top, NEXT (p));
        break;
      }
    case END_SYMBOL:
    case OCCA_SYMBOL:
    case CLOSE_SYMBOL:
    case FORMAT_CLOSE_SYMBOL:
    case BUS_SYMBOL:
    case FI_SYMBOL:
    case ESAC_SYMBOL:
    case OD_SYMBOL:
      {
        return (p);
      }
    default:
      {
        ignore_token = A68_TRUE;
      }
    }
    if (ignore_token) {
      ;
    } else if (q != NULL && WHETHER (q, ket)) {
      p = q;
    } else if (q == NULL) {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS, strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS);
      longjmp (top_down_crash_exit, 1);
    } else {
      char *diag = bracket_check_diagnose (top);
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_PARENTHESIS_2, ATTRIBUTE (q), LINE (q), ket, strlen (diag) > 0 ? diag : INFO_MISSING_KEYWORDS);
      longjmp (top_down_crash_exit, 1);
    }
  }
  return (NULL);
}

/*!
\brief driver for globally diagnosing non-matching tokens
\param top top node in syntax tree
**/

void check_parenthesis (NODE_T * top)
{
  if (!setjmp (top_down_crash_exit)) {
    if (bracket_check_parse (top, top) != NULL) {
      diagnostic_node (A68_SYNTAX_ERROR, top, ERROR_PARENTHESIS, INFO_MISSING_KEYWORDS, NULL);
    }
  }
}

/*
Next is a top-down parser that branches out the basic blocks.
After this we can assign symbol tables to basic blocks.
*/

/*!
\brief give diagnose from top-down parser
\param start embedding clause starts here
\param posit error issued at this point
\param clause type of clause being processed
\param expected token expected
**/

static void top_down_diagnose (NODE_T * start, NODE_T * posit, int clause, int expected)
{
  NODE_T *issue = (posit != NULL ? posit : start);
  if (expected != 0) {
    diagnostic_node (A68_SYNTAX_ERROR, issue, ERROR_EXPECTED_NEAR, expected, clause, SYMBOL (start), start->info->line, NULL);
  } else {
    diagnostic_node (A68_SYNTAX_ERROR, issue, ERROR_UNBALANCED_KEYWORD, clause, SYMBOL (start), LINE (start), NULL);
  }
}

/*!
\brief check for premature exhaustion of tokens
\param p position in tree
\param q
**/

static void tokens_exhausted (NODE_T * p, NODE_T * q)
{
  if (p == NULL) {
    diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
  }
}

/* This part specifically branches out loop clauses. */

/*!
\brief whether in cast or formula with loop clause
\param p position in tree
\return number of symbols to skip
**/

static int whether_loop_cast_formula (NODE_T * p)
{
/* Accept declarers that can appear in such casts but not much more. */
  if (WHETHER (p, VOID_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, INT_SYMBOL)) {
    return (1);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (1);
  } else if (whether_one_of (p, OPERATOR, BOLD_TAG, NULL_ATTRIBUTE)) {
    return (1);
  } else if (whether (p, UNION_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
    return (2);
  } else if (whether_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, NULL_ATTRIBUTE)) {
    int k;
    for (k = 0; p != NULL && (whether_one_of (p, OPEN_SYMBOL, SUB_SYMBOL, NULL_ATTRIBUTE)); FORWARD (p), k++) {
      ;
    }
    return (p != NULL && whether (p, UNION_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE) ? k : 0);
  }
  return (0);
}

/*!
\brief skip a unit in a loop clause (FROM u BY u TO u)
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_skip_loop_unit (NODE_T * p)
{
/* Unit may start with, or consist of, a loop. */
  if (whether_loop_keyword (p)) {
    p = top_down_loop (p);
  }
/* Skip rest of unit. */
  while (p != NULL) {
    int k = whether_loop_cast_formula (p);
    if (k != 0) {
/* operator-cast series ... */
      while (p != NULL && k != 0) {
        while (k != 0) {
          FORWARD (p);
          k--;
        }
        k = whether_loop_cast_formula (p);
      }
/* ... may be followed by a loop clause. */
      if (whether_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (whether_loop_keyword (p) || WHETHER (p, OD_SYMBOL)) {
/* new loop or end-of-loop. */
      return (p);
    } else if (WHETHER (p, COLON_SYMBOL)) {
      FORWARD (p);
/* skip routine header: loop clause. */
      if (p != NULL && whether_loop_keyword (p)) {
        p = top_down_loop (p);
      }
    } else if (whether_one_of (p, SEMI_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE) || WHETHER (p, EXIT_SYMBOL)) {
/* Statement separators. */
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NULL);
}

/*!
\brief skip a loop clause
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_skip_loop_series (NODE_T * p)
{
  BOOL_T siga;
  do {
    p = top_down_skip_loop_unit (p);
    siga = (BOOL_T) (p != NULL && (whether_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, COLON_SYMBOL, NULL_ATTRIBUTE)));
    if (siga) {
      FORWARD (p);
    }
  } while (!(p == NULL || !siga));
  return (p);
}

/*!
\brief make branch of loop parts
\param p position in tree
\return token from where to proceed or NULL
**/

NODE_T *top_down_loop (NODE_T * p)
{
  NODE_T *start = p, *q = p, *save;
  if (WHETHER (q, FOR_SYMBOL)) {
    tokens_exhausted (FORWARD (q), start);
    if (WHETHER (q, IDENTIFIER)) {
      ATTRIBUTE (q) = DEFINING_IDENTIFIER;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, IDENTIFIER);
      longjmp (top_down_crash_exit, 1);
    }
    tokens_exhausted (FORWARD (q), start);
    if (whether_one_of (q, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
  }
  if (WHETHER (q, FROM_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_unit (NEXT (q));
    tokens_exhausted (q, start);
    if (whether_one_of (q, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), FROM_SYMBOL);
  }
  if (WHETHER (q, BY_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (whether_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, WHILE_SYMBOL, NULL_ATTRIBUTE)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), BY_SYMBOL);
  }
  if (whether_one_of (q, TO_SYMBOL, DOWNTO_SYMBOL, NULL_ATTRIBUTE)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, WHILE_SYMBOL)) {
      ;
    } else if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, NULL_ATTRIBUTE);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), TO_SYMBOL);
  }
  if (WHETHER (q, WHILE_SYMBOL)) {
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER (q, DO_SYMBOL)) {
      ATTRIBUTE (q) = ALT_DO_SYMBOL;
    } else {
      top_down_diagnose (start, q, LOOP_CLAUSE, DO_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, PREVIOUS (q), WHILE_SYMBOL);
  }
  if (whether_one_of (q, DO_SYMBOL, ALT_DO_SYMBOL, NULL_ATTRIBUTE)) {
    int k = ATTRIBUTE (q);
    start = q;
    q = top_down_skip_loop_series (NEXT (q));
    tokens_exhausted (q, start);
    if (WHETHER_NOT (q, OD_SYMBOL)) {
      top_down_diagnose (start, q, LOOP_CLAUSE, OD_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (start, q, k);
  }
  save = NEXT (start);
  make_sub (p, start, LOOP_CLAUSE);
  return (save);
}

/*!
\brief driver for making branches of loop parts
\param p position in tree
**/

static void top_down_loops (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (SUB (q) != NULL) {
      top_down_loops (SUB (q));
    }
  }
  q = p;
  while (q != NULL) {
    if (whether_loop_keyword (q) != NULL_ATTRIBUTE) {
      q = top_down_loop (q);
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief driver for making branches of until parts
\param p position in tree
**/

static void top_down_untils (NODE_T * p)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (SUB (q) != NULL) {
      top_down_untils (SUB (q));
    }
  }
  q = p;
  while (q != NULL) {
    if (WHETHER (q, UNTIL_SYMBOL)) {
      NODE_T *u = q;
      while (NEXT (u) != NULL) {
        FORWARD (u);
      }
      make_sub (q, PREVIOUS (u), UNTIL_SYMBOL);
      return;
    } else {
      FORWARD (q);
    }
  }
}

/* Branch anything except parts of a loop. */

/*!
\brief skip serial/enquiry clause (unit series)
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_series (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    p = top_down_skip_unit (p);
    if (p != NULL) {
      if (whether_one_of (p, SEMI_SYMBOL, EXIT_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
        siga = A68_TRUE;
        FORWARD (p);
      }
    }
  }
  return (p);
}

/*!
\brief make branch of BEGIN .. END
\param begin_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_begin (NODE_T * begin_p)
{
  NODE_T *end_p = top_down_series (NEXT (begin_p));
  if (end_p == NULL || WHETHER_NOT (end_p, END_SYMBOL)) {
    top_down_diagnose (begin_p, end_p, ENCLOSED_CLAUSE, END_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  } else {
    make_sub (begin_p, end_p, BEGIN_SYMBOL);
    return (NEXT (begin_p));
  }
}

/*!
\brief make branch of CODE .. EDOC
\param code_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_code (NODE_T * code_p)
{
  NODE_T *edoc_p = top_down_series (NEXT (code_p));
  if (edoc_p == NULL || WHETHER_NOT (edoc_p, EDOC_SYMBOL)) {
    diagnostic_node (A68_SYNTAX_ERROR, code_p, ERROR_KEYWORD);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  } else {
    make_sub (code_p, edoc_p, CODE_SYMBOL);
    return (NEXT (code_p));
  }
}

/*!
\brief make branch of ( .. )
\param open_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_open (NODE_T * open_p)
{
  NODE_T *then_bar_p = top_down_series (NEXT (open_p)), *elif_bar_p;
  if (then_bar_p != NULL && WHETHER (then_bar_p, CLOSE_SYMBOL)) {
    make_sub (open_p, then_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (then_bar_p == NULL || WHETHER_NOT (then_bar_p, THEN_BAR_SYMBOL)) {
    top_down_diagnose (open_p, then_bar_p, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (open_p, PREVIOUS (then_bar_p), OPEN_SYMBOL);
  elif_bar_p = top_down_series (NEXT (then_bar_p));
  if (elif_bar_p != NULL && WHETHER (elif_bar_p, CLOSE_SYMBOL)) {
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (elif_bar_p != NULL && WHETHER (elif_bar_p, THEN_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_series (NEXT (elif_bar_p));
    if (close_p == NULL || WHETHER_NOT (close_p, CLOSE_SYMBOL)) {
      top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    }
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (elif_bar_p, PREVIOUS (close_p), THEN_BAR_SYMBOL);
    make_sub (open_p, close_p, OPEN_SYMBOL);
    return (NEXT (open_p));
  }
  if (elif_bar_p != NULL && WHETHER (elif_bar_p, ELSE_BAR_SYMBOL)) {
    NODE_T *close_p = top_down_open (elif_bar_p);
    make_sub (then_bar_p, PREVIOUS (elif_bar_p), THEN_BAR_SYMBOL);
    make_sub (open_p, elif_bar_p, OPEN_SYMBOL);
    return (close_p);
  } else {
    top_down_diagnose (open_p, elif_bar_p, ENCLOSED_CLAUSE, CLOSE_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of [ .. ]
\param sub_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_sub (NODE_T * sub_p)
{
  NODE_T *bus_p = top_down_series (NEXT (sub_p));
  if (bus_p != NULL && WHETHER (bus_p, BUS_SYMBOL)) {
    make_sub (sub_p, bus_p, SUB_SYMBOL);
    return (NEXT (sub_p));
  } else {
    top_down_diagnose (sub_p, bus_p, 0, BUS_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of { .. }
\param acco_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_acco (NODE_T * acco_p)
{
  NODE_T *occa_p = top_down_series (NEXT (acco_p));
  if (occa_p != NULL && WHETHER (occa_p, OCCA_SYMBOL)) {
    make_sub (acco_p, occa_p, ACCO_SYMBOL);
    return (NEXT (acco_p));
  } else {
    top_down_diagnose (acco_p, occa_p, ENCLOSED_CLAUSE, OCCA_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of IF .. THEN .. ELSE .. FI
\param if_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_if (NODE_T * if_p)
{
  NODE_T *then_p = top_down_series (NEXT (if_p)), *elif_p;
  if (then_p == NULL || WHETHER_NOT (then_p, THEN_SYMBOL)) {
    top_down_diagnose (if_p, then_p, CONDITIONAL_CLAUSE, THEN_SYMBOL);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (if_p, PREVIOUS (then_p), IF_SYMBOL);
  elif_p = top_down_series (NEXT (then_p));
  if (elif_p != NULL && WHETHER (elif_p, FI_SYMBOL)) {
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return (NEXT (if_p));
  }
  if (elif_p != NULL && WHETHER (elif_p, ELSE_SYMBOL)) {
    NODE_T *fi_p = top_down_series (NEXT (elif_p));
    if (fi_p == NULL || WHETHER_NOT (fi_p, FI_SYMBOL)) {
      top_down_diagnose (if_p, fi_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    } else {
      make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
      make_sub (elif_p, PREVIOUS (fi_p), ELSE_SYMBOL);
      make_sub (if_p, fi_p, IF_SYMBOL);
      return (NEXT (if_p));
    }
  }
  if (elif_p != NULL && WHETHER (elif_p, ELIF_SYMBOL)) {
    NODE_T *fi_p = top_down_if (elif_p);
    make_sub (then_p, PREVIOUS (elif_p), THEN_SYMBOL);
    make_sub (if_p, elif_p, IF_SYMBOL);
    return (fi_p);
  } else {
    top_down_diagnose (if_p, elif_p, CONDITIONAL_CLAUSE, FI_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief make branch of CASE .. IN .. OUT .. ESAC
\param case_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_case (NODE_T * case_p)
{
  NODE_T *in_p = top_down_series (NEXT (case_p)), *ouse_p;
  if (in_p == NULL || WHETHER_NOT (in_p, IN_SYMBOL)) {
    top_down_diagnose (case_p, in_p, ENCLOSED_CLAUSE, IN_SYMBOL);
    longjmp (top_down_crash_exit, 1);
  }
  make_sub (case_p, PREVIOUS (in_p), CASE_SYMBOL);
  ouse_p = top_down_series (NEXT (in_p));
  if (ouse_p != NULL && WHETHER (ouse_p, ESAC_SYMBOL)) {
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return (NEXT (case_p));
  }
  if (ouse_p != NULL && WHETHER (ouse_p, OUT_SYMBOL)) {
    NODE_T *esac_p = top_down_series (NEXT (ouse_p));
    if (esac_p == NULL || WHETHER_NOT (esac_p, ESAC_SYMBOL)) {
      top_down_diagnose (case_p, esac_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
      longjmp (top_down_crash_exit, 1);
    } else {
      make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
      make_sub (ouse_p, PREVIOUS (esac_p), OUT_SYMBOL);
      make_sub (case_p, esac_p, CASE_SYMBOL);
      return (NEXT (case_p));
    }
  }
  if (ouse_p != NULL && WHETHER (ouse_p, OUSE_SYMBOL)) {
    NODE_T *esac_p = top_down_case (ouse_p);
    make_sub (in_p, PREVIOUS (ouse_p), IN_SYMBOL);
    make_sub (case_p, ouse_p, CASE_SYMBOL);
    return (esac_p);
  } else {
    top_down_diagnose (case_p, ouse_p, ENCLOSED_CLAUSE, ESAC_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief skip a unit
\param p position in tree
\return token from where to proceed or NULL
**/

NODE_T *top_down_skip_unit (NODE_T * p)
{
  while (p != NULL && !whether_unit_terminator (p)) {
    if (WHETHER (p, BEGIN_SYMBOL)) {
      p = top_down_begin (p);
    } else if (WHETHER (p, SUB_SYMBOL)) {
      p = top_down_sub (p);
    } else if (WHETHER (p, OPEN_SYMBOL)) {
      p = top_down_open (p);
    } else if (WHETHER (p, IF_SYMBOL)) {
      p = top_down_if (p);
    } else if (WHETHER (p, CASE_SYMBOL)) {
      p = top_down_case (p);
    } else if (WHETHER (p, CODE_SYMBOL)) {
      p = top_down_code (p);
    } else if (WHETHER (p, ACCO_SYMBOL)) {
      p = top_down_acco (p);
    } else {
      FORWARD (p);
    }
  }
  return (p);
}

static NODE_T *top_down_skip_format (NODE_T *);

/*!
\brief make branch of ( .. ) in a format
\param open_p
\return token from where to proceed or NULL
**/

static NODE_T *top_down_format_open (NODE_T * open_p)
{
  NODE_T *close_p = top_down_skip_format (NEXT (open_p));
  if (close_p != NULL && WHETHER (close_p, FORMAT_CLOSE_SYMBOL)) {
    make_sub (open_p, close_p, FORMAT_OPEN_SYMBOL);
    return (NEXT (open_p));
  } else {
    top_down_diagnose (open_p, close_p, 0, FORMAT_CLOSE_SYMBOL);
    longjmp (top_down_crash_exit, 1);
    return (NULL);
  }
}

/*!
\brief skip a format text
\param p position in tree
\return token from where to proceed or NULL
**/

static NODE_T *top_down_skip_format (NODE_T * p)
{
  while (p != NULL) {
    if (WHETHER (p, FORMAT_OPEN_SYMBOL)) {
      p = top_down_format_open (p);
    } else if (whether_one_of (p, FORMAT_CLOSE_SYMBOL, FORMAT_DELIMITER_SYMBOL, NULL_ATTRIBUTE)) {
      return (p);
    } else {
      FORWARD (p);
    }
  }
  return (NULL);
}

/*!
\brief make branch of $ .. $
\param p position in tree
\return token from where to proceed or NULL
**/

static void top_down_formats (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (SUB (q) != NULL) {
      top_down_formats (SUB (q));
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, FORMAT_DELIMITER_SYMBOL)) {
      NODE_T *f = NEXT (q);
      while (f != NULL && WHETHER_NOT (f, FORMAT_DELIMITER_SYMBOL)) {
        if (WHETHER (f, FORMAT_OPEN_SYMBOL)) {
          f = top_down_format_open (f);
        } else {
          f = NEXT (f);
        }
      }
      if (f == NULL) {
        top_down_diagnose (p, f, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL);
        longjmp (top_down_crash_exit, 1);
      } else {
        make_sub (q, f, FORMAT_DELIMITER_SYMBOL);
      }
    }
  }
}

/*!
\brief make branches of phrases for the bottom-up parser
\param p position in tree
**/

void top_down_parser (NODE_T * p)
{
  if (p != NULL) {
    if (!setjmp (top_down_crash_exit)) {
      (void) top_down_series (p);
      top_down_loops (p);
      top_down_untils (p);
      top_down_formats (p);
    }
  }
}

/*
Next part is the bottom-up parser, that parses without knowing about modes while
parsing and reducing. It can therefore not exchange "[]" with "()" as was
blessed by the Revised Report. This is solved by treating CALL and SLICE as
equivalent here and letting the mode checker sort it out.

This is a Mailloux-type parser, in the sense that it scans a "phrase" for
definitions before it starts parsing, and therefore allows for tags to be used
before they are defined, which gives some freedom in top-down programming.

This parser sees the program as a set of "phrases" that needs reducing from
the inside out (bottom up). For instance

		IF a = b THEN RE a ELSE  pi * (IM a - IM b) FI
 Phrase level 3 			      +-----------+
 Phrase level 2    +---+      +--+       +----------------+
 Phrase level 1 +--------------------------------------------+

Roughly speaking, the BU parser will first work out level 3, than level 2, and
finally the level 1 phrase.
*/

/*!
\brief detect redefined keyword
\param p position in tree
*/

static void detect_redefined_keyword (NODE_T * p, int construct)
{
  if (p != NULL && whether (p, KEYWORD, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_REDEFINED_KEYWORD, SYMBOL (p), construct, NULL);
  }
}

/*!
\brief whether a series is serial or collateral
\param p position in tree
\return whether a series is serial or collateral
**/

static int serial_or_collateral (NODE_T * p)
{
  NODE_T *q;
  int semis = 0, commas = 0, exits = 0;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, COMMA_SYMBOL)) {
      commas++;
    } else if (WHETHER (q, SEMI_SYMBOL)) {
      semis++;
    } else if (WHETHER (q, EXIT_SYMBOL)) {
      exits++;
    }
  }
  if (semis == 0 && exits == 0 && commas > 0) {
    return (COLLATERAL_CLAUSE);
  } else if ((semis > 0 || exits > 0) && commas == 0) {
    return (SERIAL_CLAUSE);
  } else if (semis == 0 && exits == 0 && commas == 0) {
    return (SERIAL_CLAUSE);
  } else {
/* Heuristic guess to give intelligible error message. */
    return ((semis + exits) >= commas ? SERIAL_CLAUSE : COLLATERAL_CLAUSE);
  }
}

/*!
\brief insert a node with attribute "a" after "p"
\param p position in tree
\param a attribute
**/

static void pad_node (NODE_T * p, int a)
{
/*
This is used to fill information that Algol 68 does not require to be present.
Filling in gives one format for such construct; this helps later passes.
*/
  NODE_T *z = new_node ();
  *z = *p;
  if (GENIE (p) != NULL) {
    GENIE (z) = new_genie_info ();
  }
  PREVIOUS (z) = p;
  SUB (z) = NULL;
  ATTRIBUTE (z) = a;
  MOID (z) = NULL;
  if (NEXT (z) != NULL) {
    PREVIOUS (NEXT (z)) = z;
  }
  NEXT (p) = z;
}

/*!
\brief diagnose extensions
\param p position in tree
**/

static void a68_extension (NODE_T * p)
{
  if (program.options.portcheck) {
    diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_EXTENSION, NULL);
  } else {
    diagnostic_node (A68_WARNING, p, WARNING_EXTENSION, NULL);
  }
}

/*!
\brief diagnose for clauses not yielding a value
\param p position in tree
**/

static void empty_clause (NODE_T * p)
{
  diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_CLAUSE_WITHOUT_VALUE);
}

#if ! defined ENABLE_PAR_CLAUSE

/*!
\brief diagnose for parallel clause
\param p position in tree
**/

static void par_clause (NODE_T * p)
{
  diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_NO_PARALLEL_CLAUSE);
}

#endif

/*!
\brief diagnose for missing symbol
\param p position in tree
**/

static void strange_tokens (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_STRANGE_TOKENS);
}

/*!
\brief diagnose for strange separator
\param p position in tree
**/

static void strange_separator (NODE_T * p)
{
  NODE_T *q = (p != NULL && NEXT (p) != NULL) ? NEXT (p) : p;
  diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_STRANGE_SEPARATOR);
}

/*!
\brief whether match and reduce a sentence
\param p token where to start matching
\param a if not NULL, procedure to execute upon match
\param z if not NULL, to be set to TRUE upon match
**/

static void try_reduction (NODE_T * p, void (*a) (NODE_T *), BOOL_T * z, ...)
{
  va_list list;
  int result, arg;
  NODE_T *head = p, *tail = NULL;
  va_start (list, z);
  result = va_arg (list, int);
  while ((arg = va_arg (list, int)) != NULL_ATTRIBUTE)
  {
    BOOL_T keep_matching;
    if (p == NULL) {
      keep_matching = A68_FALSE;
    } else if (arg == WILDCARD) {
/* WILDCARD matches any Algol68G non terminal, but no keyword. */
      keep_matching = (BOOL_T) (non_terminal_string (edit_line, ATTRIBUTE (p)) != NULL);
    } else {
      if (arg >= 0) {
        keep_matching = (BOOL_T) (arg == ATTRIBUTE (p));
      } else {
        keep_matching = (BOOL_T) (arg != ATTRIBUTE (p));
      }
    }
    if (keep_matching) {
      tail = p;
      FORWARD (p);
    } else {
      va_end (list);
      return;
    }
  }
/* Print parser reductions. */
  if (head != NULL && program.options.reductions && LINE_NUMBER (head) > 0) {
    NODE_T *q;
    int count = 0;
    reductions++;
    where_in_source (STDOUT_FILENO, head);
    ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, "\nReduction %d: %s<-", reductions, non_terminal_string (edit_line, result)) >= 0);
    WRITE (STDOUT_FILENO, output_line);
    for (q = head; q != NULL && tail != NULL && q != NEXT (tail); FORWARD (q), count++) {
      int gatt = ATTRIBUTE (q);
      char *str = non_terminal_string (input_line, gatt);
      if (count > 0) {
        WRITE (STDOUT_FILENO, ", ");
      }
      if (str != NULL) {
        WRITE (STDOUT_FILENO, str);
        if (gatt == IDENTIFIER || gatt == OPERATOR || gatt == DENOTATION || gatt == INDICANT) {
          ASSERT (snprintf (output_line, (size_t) BUFFER_SIZE, " \"%s\"", SYMBOL (q)) >= 0);
          WRITE (STDOUT_FILENO, output_line);
        }
      } else {
        WRITE (STDOUT_FILENO, SYMBOL (q));
      }
    }
  }
/* Make reduction. */
  if (a != NULL) {
    a (head);
  }
  make_sub (head, tail, result);
  va_end (list);
  if (z != NULL) {
    *z = A68_TRUE;
  }
}

/*!
\brief driver for the bottom-up parser
\param p position in tree
**/

void bottom_up_parser (NODE_T * p)
{
  if (p != NULL) {
    if (!setjmp (bottom_up_crash_exit)) {
      ignore_superfluous_semicolons (p);
      reduce_particular_program (p);
    }
  }
}

/*!
\brief top-level reduction
\param p position in tree
**/

static void reduce_particular_program (NODE_T * p)
{
  NODE_T *q;
  int error_count_0 = program.error_count;
/* A program is "label sequence; particular program" */
  extract_labels (p, SERIAL_CLAUSE /* a fake here, but ok. */ );
/* Parse the program itself. */
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    if (SUB (q) != NULL) {
      reduce_subordinate (q, SOME_CLAUSE);
    }
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
    }
  }
/* Determine the encompassing enclosed clause. */
  for (q = p; q != NULL; FORWARD (q)) {
#if defined ENABLE_PAR_CLAUSE
    try_reduction (q, NULL, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#else
    try_reduction (q, par_clause, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#endif
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, LOOP_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CODE_CLAUSE, NULL_ATTRIBUTE);
  }
/* Try reducing the particular program. */
  q = p;
  try_reduction (q, NULL, NULL, PARTICULAR_PROGRAM, LABEL, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, PARTICULAR_PROGRAM, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
  if (SUB (p) == NULL || NEXT (p) != NULL) {
    recover_from_error (p, PARTICULAR_PROGRAM, (BOOL_T) ((program.error_count - error_count_0) > MAX_ERRORS));
  }
}

/*!
\brief reduce the sub-phrase that starts one level down
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_subordinate (NODE_T * p, int expect)
{
/*
If this is unsuccessful then it will at least copy the resulting attribute
as the parser can repair some faults. This gives less spurious diagnostics.
*/
  if (p != NULL) {
    if (SUB (p) != NULL) {
      BOOL_T no_error = reduce_phrase (SUB (p), expect);
      ATTRIBUTE (p) = ATTRIBUTE (SUB (p));
      if (no_error) {
        SUB (p) = SUB_SUB (p);
      }
    }
  }
}

/*!
\brief driver for reducing a phrase
\param p position in tree
\param expect information the parser may have on what is expected
\return whether errors occured
**/

BOOL_T reduce_phrase (NODE_T * p, int expect)
{
  int error_count_0 = program.error_count, error_count_02;
  BOOL_T declarer_pack = (BOOL_T) (expect == STRUCTURE_PACK || expect == PARAMETER_PACK || expect == FORMAL_DECLARERS || expect == UNION_PACK || expect == SPECIFIER);
/* Sample all info needed to decide whether a bold tag is operator or indicant. */
  extract_indicants (p);
  if (!declarer_pack) {
    extract_priorities (p);
    extract_operators (p);
  }
  error_count_02 = program.error_count;
  elaborate_bold_tags (p);
  if ((program.error_count - error_count_02) > 0) {
    longjmp (bottom_up_crash_exit, 1);
  }
/* Now we can reduce declarers, knowing which bold tags are indicants. */
  reduce_declarers (p, expect);
/* Parse the phrase, as appropriate. */
  if (!declarer_pack) {
    error_count_02 = program.error_count;
    extract_declarations (p);
    if ((program.error_count - error_count_02) > 0) {
      longjmp (bottom_up_crash_exit, 1);
    }
    extract_labels (p, expect);
    reduce_deeper_clauses_driver (p);
    reduce_statements (p, expect);
    reduce_right_to_left_constructs (p);
    reduce_constructs (p, expect);
    reduce_control_structure (p, expect);
  }
/* Do something intelligible if parsing failed. */
  if (SUB (p) == NULL || NEXT (p) != NULL) {
    recover_from_error (p, expect, (BOOL_T) ((program.error_count - error_count_0) > MAX_ERRORS));
    return (A68_FALSE);
  } else {
    return (A68_TRUE);
  }
}

/*!
\brief driver for reducing declarers
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_declarers (NODE_T * p, int expect)
{
  reduce_lengtheties (p);
  reduce_indicants (p);
  reduce_small_declarers (p);
  reduce_declarer_lists (p);
  reduce_row_proc_op_declarers (p);
  if (expect == STRUCTURE_PACK) {
    reduce_struct_pack (p);
  } else if (expect == PARAMETER_PACK) {
    reduce_parameter_pack (p);
  } else if (expect == FORMAL_DECLARERS) {
    reduce_formal_declarer_pack (p);
  } else if (expect == UNION_PACK) {
    reduce_union_pack (p);
  } else if (expect == SPECIFIER) {
    reduce_specifiers (p);
  } else {
    NODE_T *q;
    for (q = p; q != NULL; FORWARD (q)) {
      if (whether (q, OPEN_SYMBOL, COLON_SYMBOL, NULL_ATTRIBUTE) && !(expect == GENERIC_ARGUMENT || expect == BOUNDS)) {
        if (whether_one_of (p, IN_SYMBOL, THEN_BAR_SYMBOL, NULL_ATTRIBUTE)) {
          reduce_subordinate (q, SPECIFIER);
        }
      }
      if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
        reduce_subordinate (q, PARAMETER_PACK);
      }
      if (whether (q, OPEN_SYMBOL, VOID_SYMBOL, COLON_SYMBOL, NULL_ATTRIBUTE)) {
        reduce_subordinate (q, PARAMETER_PACK);
      }
    }
  }
}

/*!
\brief driver for reducing control structure elements
\param p position in tree
**/

static void reduce_deeper_clauses_driver (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (SUB (p) != NULL) {
      reduce_deeper_clauses (p);
    }
  }
}

/*!
\brief reduces PRIMARY, SECONDARY, TERITARY and FORMAT TEXT
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_statements (NODE_T * p, int expect)
{
  reduce_primary_bits (p, expect);
  if (expect != ENCLOSED_CLAUSE) {
    reduce_primaries (p, expect);
    if (expect == FORMAT_TEXT) {
      reduce_format_texts (p);
    } else {
      reduce_secondaries (p);
      reduce_formulae (p);
      reduce_tertiaries (p);
    }
  }
}

/*!
\brief handle cases that need reducing from right-to-left
\param p position in tree
**/

static void reduce_right_to_left_constructs (NODE_T * p)
{
/*
Here are cases that need reducing from right-to-left whereas many things
can be reduced left-to-right. Assignations are a notable example; one could
discuss whether it would not be more natural to write 1 =: k in stead of
k := 1. The latter is said to be more natural, or it could be just computing
history. Meanwhile we use this routine.
*/
  if (p != NULL) {
    reduce_right_to_left_constructs (NEXT (p));
/* Assignations. */
    if (WHETHER (p, TERTIARY)) {
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, JUMP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, SKIP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ASSIGNATION, TERTIARY, ASSIGN_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
    }
/* Routine texts with parameter pack. */
    else if (WHETHER (p, PARAMETER_PACK)) {
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ROUTINE_TEXT, PARAMETER_PACK, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    }
/* Routine texts without parameter pack. */
    else if (WHETHER (p, DECLARER)) {
      if (!(PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PARAMETER_PACK))) {
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, DECLARER, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      }
    } else if (WHETHER (p, VOID_SYMBOL)) {
      if (!(PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PARAMETER_PACK))) {
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ASSIGNATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, IDENTITY_RELATION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, AND_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, OR_FUNCTION, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, JUMP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, SKIP, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
        try_reduction (p, NULL, NULL, ROUTINE_TEXT, VOID_SYMBOL, COLON_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      }
    }
  }
}

/*!
\brief  graciously ignore extra semicolons
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void ignore_superfluous_semicolons (NODE_T * p)
{
/*
This routine relaxes the parser a bit with respect to superfluous semicolons,
for instance "FI; OD". These provoke only a warning.
*/
  for (; p != NULL; FORWARD (p)) {
    ignore_superfluous_semicolons (SUB (p));
    if (NEXT (p) != NULL && WHETHER (NEXT (p), SEMI_SYMBOL) && NEXT_NEXT (p) == NULL) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, NEXT (p), WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (NEXT (p)));
      NEXT (p) = NULL;
    } else if (WHETHER (p, SEMI_SYMBOL) && whether_semicolon_less (NEXT (p))) {
      diagnostic_node (A68_WARNING | A68_FORCE_DIAGNOSTICS, p, WARNING_SKIPPED_SUPERFLUOUS, ATTRIBUTE (p));
      if (PREVIOUS (p) != NULL) {
        NEXT (PREVIOUS (p)) = NEXT (p);
      }
      PREVIOUS (NEXT (p)) = PREVIOUS (p);
    }
  }
}

/*!
\brief reduce constructs in proper order
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_constructs (NODE_T * p, int expect)
{
  reduce_basic_declarations (p);
  reduce_units (p);
  reduce_erroneous_units (p);
  if (expect != UNIT) {
    if (expect == GENERIC_ARGUMENT) {
      reduce_generic_arguments (p);
    } else if (expect == BOUNDS) {
      reduce_bounds (p);
    } else {
      reduce_declaration_lists (p);
      if (expect != DECLARATION_LIST) {
        reduce_labels (p);
        if (expect == SOME_CLAUSE) {
          expect = serial_or_collateral (p);
        }
        if (expect == SERIAL_CLAUSE) {
          reduce_serial_clauses (p);
        } else if (expect == ENQUIRY_CLAUSE) {
          reduce_enquiry_clauses (p);
        } else if (expect == COLLATERAL_CLAUSE) {
          reduce_collateral_clauses (p);
        } else if (expect == ARGUMENT) {
          reduce_arguments (p);
        }
      }
    }
  }
}

/*!
\brief reduce control structure
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_control_structure (NODE_T * p, int expect)
{
  reduce_enclosed_clause_bits (p, expect);
  reduce_enclosed_clauses (p);
}

/*!
\brief reduce lengths in declarers
\param p position in tree
**/

static void reduce_lengtheties (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    try_reduction (q, NULL, NULL, LONGETY, LONG_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SHORTETY, SHORT_SYMBOL, NULL_ATTRIBUTE);
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, LONGETY, LONGETY, LONG_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, SHORTETY, SHORTETY, SHORT_SYMBOL, NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief reduce indicants
\param p position in tree
**/

static void reduce_indicants (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INDICANT, INT_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, REAL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, BITS_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, BYTES_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, COMPLEX_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, COMPL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, BOOL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, CHAR_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, FORMAT_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, STRING_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, FILE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, CHANNEL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, SEMA_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, PIPE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INDICANT, SOUND_SYMBOL, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce basic declarations, like LONG BITS, STRING, ..
\param p position in tree
**/

static void reduce_small_declarers (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (whether (q, LONGETY, INDICANT, NULL_ATTRIBUTE)) {
      int a;
      if (SUB_NEXT (q) == NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
      } else {
        a = ATTRIBUTE (SUB_NEXT (q));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
        } else {
          diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
        }
      }
    } else if (whether (q, SHORTETY, INDICANT, NULL_ATTRIBUTE)) {
      int a;
      if (SUB_NEXT (q) == NULL) {
        diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
        try_reduction (q, NULL, NULL, DECLARER, SHORTETY, INDICANT, NULL_ATTRIBUTE);
      } else {
        a = ATTRIBUTE (SUB_NEXT (q));
        if (a == INT_SYMBOL || a == REAL_SYMBOL || a == BITS_SYMBOL || a == BYTES_SYMBOL || a == COMPLEX_SYMBOL || a == COMPL_SYMBOL) {
          try_reduction (q, NULL, NULL, DECLARER, SHORTETY, INDICANT, NULL_ATTRIBUTE);
        } else {
          diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_EXPECTED, INFO_APPROPRIATE_DECLARER);
          try_reduction (q, NULL, NULL, DECLARER, LONGETY, INDICANT, NULL_ATTRIBUTE);
        }
      }
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, DECLARER, INDICANT, NULL_ATTRIBUTE);
  }
}

/*!
\brief whether formal bounds
\param p position in tree
\return whether formal bounds
**/

static BOOL_T whether_formal_bounds (NODE_T * p)
{
  if (p == NULL) {
    return (A68_TRUE);
  } else {
    switch (ATTRIBUTE (p)) {
    case OPEN_SYMBOL:
    case CLOSE_SYMBOL:
    case SUB_SYMBOL:
    case BUS_SYMBOL:
    case COMMA_SYMBOL:
    case COLON_SYMBOL:
    case DOTDOT_SYMBOL:
    case INT_DENOTATION:
    case IDENTIFIER:
    case OPERATOR:
      {
        return ((BOOL_T) (whether_formal_bounds (SUB (p)) && whether_formal_bounds (NEXT (p))));
      }
    default:
      {
        return (A68_FALSE);
      }
    }
  }
}

/*!
\brief reduce declarer lists for packs
\param p position in tree
**/

static void reduce_declarer_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (NEXT (q) != NULL && SUB_NEXT (q) != NULL) {
      if (WHETHER (q, STRUCT_SYMBOL)) {
        reduce_subordinate (NEXT (q), STRUCTURE_PACK);
        try_reduction (q, NULL, NULL, DECLARER, STRUCT_SYMBOL, STRUCTURE_PACK, NULL_ATTRIBUTE);
      } else if (WHETHER (q, UNION_SYMBOL)) {
        reduce_subordinate (NEXT (q), UNION_PACK);
        try_reduction (q, NULL, NULL, DECLARER, UNION_SYMBOL, UNION_PACK, NULL_ATTRIBUTE);
      } else if (WHETHER (q, PROC_SYMBOL)) {
        if (whether (q, PROC_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
          if (!whether_formal_bounds (SUB_NEXT (q))) {
            reduce_subordinate (NEXT (q), FORMAL_DECLARERS);
          }
        }
      } else if (WHETHER (q, OP_SYMBOL)) {
        if (whether (q, OP_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
          if (!whether_formal_bounds (SUB_NEXT (q))) {
            reduce_subordinate (NEXT (q), FORMAL_DECLARERS);
          }
        }
      }
    }
  }
}

/*!
\brief reduce ROW, PROC and OP declarers
\param p position in tree
**/

static void reduce_row_proc_op_declarers (NODE_T * p)
{
  BOOL_T siga = A68_TRUE;
  while (siga) {
    NODE_T *q;
    siga = A68_FALSE;
    for (q = p; q != NULL; FORWARD (q)) {
/* FLEX DECL. */
      if (whether (q, FLEX_SYMBOL, DECLARER, NULL_ATTRIBUTE)) {
        try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      }
/* FLEX [] DECL. */
      if (whether (q, FLEX_SYMBOL, SUB_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB_NEXT (q) != NULL) {
        reduce_subordinate (NEXT (q), BOUNDS);
        try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
      }
/* FLEX () DECL. */
      if (whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB_NEXT (q) != NULL) {
        if (!whether (q, FLEX_SYMBOL, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
          reduce_subordinate (NEXT (q), BOUNDS);
          try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, BOUNDS, DECLARER, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, DECLARER, FLEX_SYMBOL, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
        }
      }
/* [] DECL. */
      if (whether (q, SUB_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB (q) != NULL) {
        reduce_subordinate (q, BOUNDS);
        try_reduction (q, NULL, &siga, DECLARER, BOUNDS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
      }
/* () DECL. */
      if (whether (q, OPEN_SYMBOL, DECLARER, NULL_ATTRIBUTE) && SUB (q) != NULL) {
        if (whether (q, OPEN_SYMBOL, DECLARER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
/* Catch e.g. (INT i) () INT: */
          if (whether_formal_bounds (SUB (q))) {
            reduce_subordinate (q, BOUNDS);
            try_reduction (q, NULL, &siga, DECLARER, BOUNDS, DECLARER, NULL_ATTRIBUTE);
            try_reduction (q, NULL, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
          }
        } else {
          reduce_subordinate (q, BOUNDS);
          try_reduction (q, NULL, &siga, DECLARER, BOUNDS, DECLARER, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, DECLARER, FORMAL_BOUNDS, DECLARER, NULL_ATTRIBUTE);
        }
      }
    }
/* PROC DECL, PROC () DECL, OP () DECL. */
    for (q = p; q != NULL; FORWARD (q)) {
      int a = ATTRIBUTE (q);
      if (a == REF_SYMBOL) {
        try_reduction (q, NULL, &siga, DECLARER, REF_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      } else if (a == PROC_SYMBOL) {
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, DECLARER, PROC_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, NULL_ATTRIBUTE);
      } else if (a == OP_SYMBOL) {
        try_reduction (q, NULL, &siga, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, DECLARER, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, OPERATOR_PLAN, OP_SYMBOL, FORMAL_DECLARERS, VOID_SYMBOL, NULL_ATTRIBUTE);
      }
    }
  }
}

/*!
\brief reduce structure packs
\param p position in tree
**/

static void reduce_struct_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD, DECLARER, IDENTIFIER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD, STRUCTURED_FIELD, COMMA_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, COMMA_SYMBOL, STRUCTURED_FIELD, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD_LIST, SEMI_SYMBOL, STRUCTURED_FIELD, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, STRUCTURE_PACK, OPEN_SYMBOL, STRUCTURED_FIELD_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce parameter packs
\param p position in tree
**/

static void reduce_parameter_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, PARAMETER, DECLARER, IDENTIFIER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PARAMETER, PARAMETER, COMMA_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, PARAMETER_LIST, PARAMETER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PARAMETER_LIST, PARAMETER_LIST, COMMA_SYMBOL, PARAMETER, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, PARAMETER_PACK, OPEN_SYMBOL, PARAMETER_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce formal declarer packs
\param p position in tree
**/

static void reduce_formal_declarer_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, FORMAL_DECLARERS_LIST, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, COMMA_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, SEMI_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, FORMAL_DECLARERS_LIST, FORMAL_DECLARERS_LIST, DECLARER, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, FORMAL_DECLARERS, OPEN_SYMBOL, FORMAL_DECLARERS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce union packs (formal declarers and VOID)
\param p position in tree
**/

static void reduce_union_pack (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, COMMA_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, SEMI_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, SEMI_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, UNION_DECLARER_LIST, UNION_DECLARER_LIST, VOID_SYMBOL, NULL_ATTRIBUTE);
    }
  }
  q = p;
  try_reduction (q, NULL, NULL, UNION_PACK, OPEN_SYMBOL, UNION_DECLARER_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce specifiers
\param p position in tree
**/

static void reduce_specifiers (NODE_T * p)
{
  NODE_T *q = p;
  try_reduction (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, DECLARER, IDENTIFIER, CLOSE_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, DECLARER, CLOSE_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, SPECIFIER, OPEN_SYMBOL, VOID_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
}

/*!
\brief reduce control structure elements
\param p position in tree
**/

static void reduce_deeper_clauses (NODE_T * p)
{
  if (WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
    reduce_subordinate (p, FORMAT_TEXT);
  } else if (WHETHER (p, FORMAT_OPEN_SYMBOL)) {
    reduce_subordinate (p, FORMAT_TEXT);
  } else if (WHETHER (p, OPEN_SYMBOL)) {
    if (NEXT (p) != NULL && WHETHER (NEXT (p), THEN_BAR_SYMBOL)) {
      reduce_subordinate (p, ENQUIRY_CLAUSE);
    } else if (PREVIOUS (p) != NULL && WHETHER (PREVIOUS (p), PAR_SYMBOL)) {
      reduce_subordinate (p, COLLATERAL_CLAUSE);
    }
  } else if (whether_one_of (p, IF_SYMBOL, ELIF_SYMBOL, CASE_SYMBOL, OUSE_SYMBOL, WHILE_SYMBOL, UNTIL_SYMBOL, ELSE_BAR_SYMBOL, ACCO_SYMBOL, NULL_ATTRIBUTE)) {
    reduce_subordinate (p, ENQUIRY_CLAUSE);
  } else if (WHETHER (p, BEGIN_SYMBOL)) {
    reduce_subordinate (p, SOME_CLAUSE);
  } else if (whether_one_of (p, THEN_SYMBOL, ELSE_SYMBOL, OUT_SYMBOL, DO_SYMBOL, ALT_DO_SYMBOL, CODE_SYMBOL, NULL_ATTRIBUTE)) {
    reduce_subordinate (p, SERIAL_CLAUSE);
  } else if (WHETHER (p, IN_SYMBOL)) {
    reduce_subordinate (p, COLLATERAL_CLAUSE);
  } else if (WHETHER (p, THEN_BAR_SYMBOL)) {
    reduce_subordinate (p, SOME_CLAUSE);
  } else if (WHETHER (p, LOOP_CLAUSE)) {
    reduce_subordinate (p, ENCLOSED_CLAUSE);
  } else if (whether_one_of (p, FOR_SYMBOL, FROM_SYMBOL, BY_SYMBOL, TO_SYMBOL, DOWNTO_SYMBOL, NULL_ATTRIBUTE)) {
    reduce_subordinate (p, UNIT);
  }
}

/*!
\brief reduce primary elements
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_primary_bits (NODE_T * p, int expect)
{
  NODE_T *q = p;
  for (; q != NULL; FORWARD (q)) {
    if (whether (q, IDENTIFIER, OF_SYMBOL, NULL_ATTRIBUTE)) {
      ATTRIBUTE (q) = FIELD_IDENTIFIER;
    }
    try_reduction (q, NULL, NULL, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, NIHIL, NIL_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SKIP, SKIP_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SELECTOR, FIELD_IDENTIFIER, OF_SYMBOL, NULL_ATTRIBUTE);
/* JUMPs without GOTO are resolved later. */
    try_reduction (q, NULL, NULL, JUMP, GOTO_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, LONGETY, INT_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, LONGETY, REAL_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, LONGETY, BITS_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, SHORTETY, INT_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, SHORTETY, REAL_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, SHORTETY, BITS_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, INT_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, REAL_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, BITS_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, TRUE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, FALSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DENOTATION, EMPTY_SYMBOL, NULL_ATTRIBUTE);
    if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
      BOOL_T siga = A68_TRUE;
      while (siga) {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, LABEL, LABEL, DEFINING_IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE);
      }
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
#if defined ENABLE_PAR_CLAUSE
    try_reduction (q, NULL, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#else
    try_reduction (q, par_clause, NULL, PARALLEL_CLAUSE, PAR_SYMBOL, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
#endif
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, PARALLEL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, LOOP_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CODE_CLAUSE, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce primaries completely
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_primaries (NODE_T * p, int expect)
{
  NODE_T *q = p;
  while (q != NULL) {
    BOOL_T fwd = A68_TRUE, siga;
/* Primaries excepts call and slice. */
    try_reduction (q, NULL, NULL, PRIMARY, IDENTIFIER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, CAST, DECLARER, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, CAST, VOID_SYMBOL, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, ASSERTION, ASSERT_SYMBOL, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, CAST, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIMARY, FORMAT_TEXT, NULL_ATTRIBUTE);
/* Call and slice. */
    siga = A68_TRUE;
    while (siga) {
      NODE_T *x = NEXT (q);
      siga = A68_FALSE;
      if (WHETHER (q, PRIMARY) && x != NULL) {
        if (WHETHER (x, OPEN_SYMBOL)) {
          reduce_subordinate (NEXT (q), GENERIC_ARGUMENT);
          try_reduction (q, NULL, &siga, SPECIFICATION, PRIMARY, GENERIC_ARGUMENT, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, PRIMARY, SPECIFICATION, NULL_ATTRIBUTE);
        } else if (WHETHER (x, SUB_SYMBOL)) {
          reduce_subordinate (NEXT (q), GENERIC_ARGUMENT);
          try_reduction (q, NULL, &siga, SPECIFICATION, PRIMARY, GENERIC_ARGUMENT, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, PRIMARY, SPECIFICATION, NULL_ATTRIBUTE);
        }
      }
    }
/* Now that call and slice are known, reduce remaining ( .. ). */
    if (WHETHER (q, OPEN_SYMBOL) && SUB (q) != NULL) {
      reduce_subordinate (q, SOME_CLAUSE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CLOSED_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, COLLATERAL_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, CONDITIONAL_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, INTEGER_CASE_CLAUSE, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, ENCLOSED_CLAUSE, UNITED_CASE_CLAUSE, NULL_ATTRIBUTE);
      if (PREVIOUS (q) != NULL) {
        q = PREVIOUS (q);
        fwd = A68_FALSE;
      }
    }
/* Format text items. */
    if (expect == FORMAT_TEXT) {
      NODE_T *r;
      for (r = p; r != NULL; FORWARD (r)) {
        try_reduction (r, NULL, NULL, DYNAMIC_REPLICATOR, FORMAT_ITEM_N, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
        try_reduction (r, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_G, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
        try_reduction (r, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_H, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
        try_reduction (r, NULL, NULL, FORMAT_PATTERN, FORMAT_ITEM_F, ENCLOSED_CLAUSE, NULL_ATTRIBUTE);
      }
    }
    if (fwd) {
      FORWARD (q);
    }
  }
}

/*!
\brief enforce that ambiguous patterns are separated by commas
\param p position in tree
**/

static void ambiguous_patterns (NODE_T * p)
{
/*
Example: printf (($+d.2d +d.2d$, 1, 2)) can produce either "+1.00 +2.00" or
"+1+002.00". A comma must be supplied to resolve the ambiguity.

The obvious thing would be to weave this into the syntax, letting the BU parser
sort it out. But the C-style patterns do not suffer from Algol 68 pattern
ambiguity, so by solving it this way we maximise freedom in writing the patterns
as we want without introducing two "kinds" of patterns, and so we have shorter
routines for implementing formatted transput. This is a pragmatic system.
*/
  NODE_T *q, *last_pat = NULL;
  for (q = p; q != NULL; FORWARD (q)) {
    switch (ATTRIBUTE (q)) {
    case INTEGRAL_PATTERN:     /* These are the potentially ambiguous patterns. */
    case REAL_PATTERN:
    case COMPLEX_PATTERN:
    case BITS_PATTERN:
      {
        if (last_pat != NULL) {
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_COMMA_MUST_SEPARATE, ATTRIBUTE (last_pat), ATTRIBUTE (q));
        }
        last_pat = q;
        break;
      }
    case COMMA_SYMBOL:
      {
        last_pat = NULL;
        break;
      }
    }
  }
}

/*!
\brief reduce format texts completely
\param p position in tree
**/

void reduce_c_pattern (NODE_T * p, int pr, int let)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, REPLICATOR, let, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, pr, FORMAT_ITEM_ESCAPE, FORMAT_ITEM_MINUS, FORMAT_ITEM_PLUS, REPLICATOR, FORMAT_ITEM_POINT, REPLICATOR, let, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce format texts completely
\param p position in tree
**/

static void reduce_format_texts (NODE_T * p)
{
  NODE_T *q;
/* Replicators. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REPLICATOR, STATIC_REPLICATOR, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REPLICATOR, DYNAMIC_REPLICATOR, NULL_ATTRIBUTE);
  }
/* "OTHER" patterns. */
  reduce_c_pattern (p, BITS_C_PATTERN, FORMAT_ITEM_B);
  reduce_c_pattern (p, BITS_C_PATTERN, FORMAT_ITEM_O);
  reduce_c_pattern (p, BITS_C_PATTERN, FORMAT_ITEM_X);
  reduce_c_pattern (p, CHAR_C_PATTERN, FORMAT_ITEM_C);
  reduce_c_pattern (p, FIXED_C_PATTERN, FORMAT_ITEM_F);
  reduce_c_pattern (p, FLOAT_C_PATTERN, FORMAT_ITEM_E);
  reduce_c_pattern (p, GENERAL_C_PATTERN, FORMAT_ITEM_G);
  reduce_c_pattern (p, INTEGRAL_C_PATTERN, FORMAT_ITEM_D);
  reduce_c_pattern (p, INTEGRAL_C_PATTERN, FORMAT_ITEM_I);
  reduce_c_pattern (p, STRING_C_PATTERN, FORMAT_ITEM_S);
/* Radix frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, RADIX_FRAME, REPLICATOR, FORMAT_ITEM_R, NULL_ATTRIBUTE);
  }
/* Insertions. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_X, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_Y, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_L, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_P, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_Q, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, FORMAT_ITEM_K, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INSERTION, LITERAL, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INSERTION, REPLICATOR, INSERTION, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, INSERTION, INSERTION, INSERTION, NULL_ATTRIBUTE);
    }
  }
/* Replicated suppressible frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_S, FORMAT_ITEM_D, NULL_ATTRIBUTE);
  }
/* Suppressible frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_D, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_E_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_E, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_POINT_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_POINT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_I_FRAME, FORMAT_ITEM_S, FORMAT_ITEM_I, NULL_ATTRIBUTE);
  }
/* Replicated frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, REPLICATOR, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, REPLICATOR, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, REPLICATOR, FORMAT_ITEM_D, NULL_ATTRIBUTE);
  }
/* Frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, FORMAT_ITEM_A, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, FORMAT_ITEM_Z, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, FORMAT_ITEM_D, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_E_FRAME, FORMAT_ITEM_E, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_POINT_FRAME, FORMAT_ITEM_POINT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_I_FRAME, FORMAT_ITEM_I, NULL_ATTRIBUTE);
  }
/* Frames with an insertion. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, FORMAT_A_FRAME, INSERTION, FORMAT_A_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_Z_FRAME, INSERTION, FORMAT_Z_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_D_FRAME, INSERTION, FORMAT_D_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_E_FRAME, INSERTION, FORMAT_E_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_POINT_FRAME, INSERTION, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMAT_I_FRAME, INSERTION, FORMAT_I_FRAME, NULL_ATTRIBUTE);
  }
/* String patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, STRING_PATTERN, REPLICATOR, FORMAT_A_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, STRING_PATTERN, FORMAT_A_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, STRING_PATTERN, STRING_PATTERN, STRING_PATTERN, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, STRING_PATTERN, STRING_PATTERN, INSERTION, STRING_PATTERN, NULL_ATTRIBUTE);
    }
  }
/* Integral moulds. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INTEGRAL_MOULD, FORMAT_Z_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INTEGRAL_MOULD, FORMAT_D_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga = A68_TRUE;
    while (siga) {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, INTEGRAL_MOULD, INTEGRAL_MOULD, INTEGRAL_MOULD, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, INTEGRAL_MOULD, INTEGRAL_MOULD, INSERTION, NULL_ATTRIBUTE);
    }
  }
/* Sign moulds. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_PLUS, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_ITEM_MINUS, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, SIGN_MOULD, FORMAT_ITEM_PLUS, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SIGN_MOULD, FORMAT_ITEM_MINUS, NULL_ATTRIBUTE);
  }
/* Exponent frames. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, EXPONENT_FRAME, FORMAT_E_FRAME, SIGN_MOULD, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, EXPONENT_FRAME, FORMAT_E_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
/* Real patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, FORMAT_POINT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, FORMAT_POINT_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, REAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, REAL_PATTERN, INTEGRAL_MOULD, EXPONENT_FRAME, NULL_ATTRIBUTE);
  }
/* Complex patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, COMPLEX_PATTERN, REAL_PATTERN, FORMAT_I_FRAME, REAL_PATTERN, NULL_ATTRIBUTE);
  }
/* Bits patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BITS_PATTERN, RADIX_FRAME, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
/* Integral patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, INTEGRAL_PATTERN, SIGN_MOULD, INTEGRAL_MOULD, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INTEGRAL_PATTERN, INTEGRAL_MOULD, NULL_ATTRIBUTE);
  }
/* Patterns. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BOOLEAN_PATTERN, FORMAT_ITEM_B, COLLECTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, CHOICE_PATTERN, FORMAT_ITEM_C, COLLECTION, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BOOLEAN_PATTERN, FORMAT_ITEM_B, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_G, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERAL_PATTERN, FORMAT_ITEM_H, NULL_ATTRIBUTE);
  }
  ambiguous_patterns (p);
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, a68_extension, NULL, A68_PATTERN, BITS_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, CHAR_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, FIXED_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, FLOAT_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, GENERAL_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, INTEGRAL_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, A68_PATTERN, STRING_C_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, BITS_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, BOOLEAN_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, CHOICE_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, COMPLEX_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, FORMAT_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, GENERAL_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, INTEGRAL_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, REAL_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, A68_PATTERN, STRING_PATTERN, NULL_ATTRIBUTE);
  }
/* Pictures. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, PICTURE, INSERTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PICTURE, A68_PATTERN, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PICTURE, COLLECTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PICTURE, REPLICATOR, COLLECTION, NULL_ATTRIBUTE);
  }
/* Picture lists. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, PICTURE)) {
      BOOL_T siga = A68_TRUE;
      try_reduction (q, NULL, NULL, PICTURE_LIST, PICTURE, NULL_ATTRIBUTE);
      while (siga) {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, PICTURE_LIST, PICTURE_LIST, COMMA_SYMBOL, PICTURE, NULL_ATTRIBUTE);
        /* We filtered ambiguous patterns, so commas may be omitted. */
        try_reduction (q, NULL, &siga, PICTURE_LIST, PICTURE_LIST, PICTURE, NULL_ATTRIBUTE);
      }
    }
  }
}

/*!
\brief reduce secondaries completely
\param p position in tree
**/

static void reduce_secondaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, SECONDARY, PRIMARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERATOR, LOC_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERATOR, HEAP_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, GENERATOR, NEW_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SECONDARY, GENERATOR, NULL_ATTRIBUTE);
  }
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; NEXT (q) != NULL; FORWARD (q)) {
      ;
    }
    for (; q != NULL; q = PREVIOUS (q)) {
      try_reduction (q, NULL, &siga, SELECTION, SELECTOR, SECONDARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, SECONDARY, SELECTION, NULL_ATTRIBUTE);
    }
  }
}

/*!
\brief whether "q" is an operator with priority "k"
\param q operator token
\param k priority
\return whether "q" is an operator with priority "k"
**/

static int operator_with_priority (NODE_T * q, int k)
{
  return (NEXT (q) != NULL && ATTRIBUTE (NEXT (q)) == OPERATOR && PRIO (NEXT (q)->info) == k);
}

/*!
\brief reduce formulae
\param p position in tree
**/

static void reduce_formulae (NODE_T * p)
{
  NODE_T *q = p;
  int priority;
  while (q != NULL) {
    if (whether_one_of (q, OPERATOR, SECONDARY, NULL_ATTRIBUTE)) {
      q = reduce_dyadic (q, NULL_ATTRIBUTE);
    } else {
      FORWARD (q);
    }
  }
/* Reduce the expression. */
  for (priority = MAX_PRIORITY; priority >= 0; priority--) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (operator_with_priority (q, priority)) {
        BOOL_T siga = A68_FALSE;
        NODE_T *op = NEXT (q);
        if (WHETHER (q, SECONDARY)) {
          try_reduction (q, NULL, &siga, FORMULA, SECONDARY, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, SECONDARY, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, SECONDARY, OPERATOR, FORMULA, NULL_ATTRIBUTE);
        } else if (WHETHER (q, MONADIC_FORMULA)) {
          try_reduction (q, NULL, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
          try_reduction (q, NULL, &siga, FORMULA, MONADIC_FORMULA, OPERATOR, FORMULA, NULL_ATTRIBUTE);
        }
        if (priority == 0 && siga) {
          diagnostic_node (A68_SYNTAX_ERROR, op, ERROR_NO_PRIORITY, NULL);
        }
        siga = A68_TRUE;
        while (siga) {
          NODE_T *op2 = NEXT (q);
          siga = A68_FALSE;
          if (operator_with_priority (q, priority)) {
            try_reduction (q, NULL, &siga, FORMULA, FORMULA, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
          }
          if (operator_with_priority (q, priority)) {
            try_reduction (q, NULL, &siga, FORMULA, FORMULA, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
          }
          if (operator_with_priority (q, priority)) {
            try_reduction (q, NULL, &siga, FORMULA, FORMULA, OPERATOR, FORMULA, NULL_ATTRIBUTE);
          }
          if (priority == 0 && siga) {
            diagnostic_node (A68_SYNTAX_ERROR, op2, ERROR_NO_PRIORITY, NULL);
          }
        }
      }
    }
  }
}

/*!
\brief reduce dyadic expressions
\param p position in tree
\param u current priority
\return token from where to continue
**/

static NODE_T *reduce_dyadic (NODE_T * p, int u)
{
/* We work inside out - higher priority expressions get reduced first. */
  if (u > MAX_PRIORITY) {
    if (p == NULL) {
      return (NULL);
    } else if (WHETHER (p, OPERATOR)) {
/* Reduce monadic formulas. */
      NODE_T *q = p;
      BOOL_T siga;
      do {
        PRIO (INFO (q)) = 10;
        siga = (BOOL_T) ((NEXT (q) != NULL) && (WHETHER (NEXT (q), OPERATOR)));
        if (siga) {
          FORWARD (q);
        }
      } while (siga);
      try_reduction (q, NULL, NULL, MONADIC_FORMULA, OPERATOR, SECONDARY, NULL_ATTRIBUTE);
      while (q != p) {
        q = PREVIOUS (q);
        try_reduction (q, NULL, NULL, MONADIC_FORMULA, OPERATOR, MONADIC_FORMULA, NULL_ATTRIBUTE);
      }
    }
    FORWARD (p);
  } else {
    p = reduce_dyadic (p, u + 1);
    while (p != NULL && WHETHER (p, OPERATOR) && PRIO (INFO (p)) == u) {
      FORWARD (p);
      p = reduce_dyadic (p, u + 1);
    }
  }
  return (p);
}

/*!
\brief reduce tertiaries completely
\param p position in tree
**/

static void reduce_tertiaries (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, TERTIARY, NIHIL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, FORMULA, MONADIC_FORMULA, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, TERTIARY, FORMULA, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, TERTIARY, SECONDARY, NULL_ATTRIBUTE);
  }
  siga = A68_TRUE;
  while (siga) {
    siga = A68_FALSE;
    for (q = p; q != NULL; FORWARD (q)) {
      try_reduction (q, NULL, &siga, TRANSPOSE_FUNCTION, TRANSPOSE_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, DIAGONAL_FUNCTION, TERTIARY, DIAGONAL_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, DIAGONAL_FUNCTION, DIAGONAL_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, COLUMN_FUNCTION, TERTIARY, COLUMN_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, COLUMN_FUNCTION, COLUMN_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, ROW_FUNCTION, TERTIARY, ROW_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, ROW_FUNCTION, ROW_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
    }
    for (q = p; q != NULL; FORWARD (q)) {
      try_reduction (q, a68_extension, &siga, TERTIARY, TRANSPOSE_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (q, a68_extension, &siga, TERTIARY, DIAGONAL_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (q, a68_extension, &siga, TERTIARY, COLUMN_FUNCTION, NULL_ATTRIBUTE);
      try_reduction (q, a68_extension, &siga, TERTIARY, ROW_FUNCTION, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, IDENTITY_RELATION, TERTIARY, IS_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, IDENTITY_RELATION, TERTIARY, ISNT_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, AND_FUNCTION, TERTIARY, ANDF_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, OR_FUNCTION, TERTIARY, ORF_SYMBOL, TERTIARY, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce declarations
\param p position in tree
**/

static void reduce_basic_declarations (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, ENVIRON_NAME, ENVIRON_SYMBOL, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PRIORITY_DECLARATION, PRIO_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
/* Errors. */
    try_reduction (q, strange_tokens, NULL, PRIORITY_DECLARATION, PRIO_SYMBOL, -DEFINING_OPERATOR, -EQUALS_SYMBOL, -PRIORITY, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, MODE_DECLARATION, MODE_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, -DECLARER, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, PROCEDURE_VARIABLE_DECLARATION, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, PROCEDURE_VARIABLE_DECLARATION, QUALIFIER, PROC_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, strange_tokens, NULL, BRIEF_OPERATOR_DECLARATION, OP_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, -ROUTINE_TEXT, NULL_ATTRIBUTE);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER. */
    try_reduction (q, strange_tokens, NULL, PROCEDURE_DECLARATION, PROC_SYMBOL, WILDCARD, ROUTINE_TEXT, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, ENVIRON_NAME, ENVIRON_NAME, COMMA_SYMBOL, ROW_CHAR_DENOTATION, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PRIORITY_DECLARATION, PRIORITY_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, PRIORITY, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, DECLARER, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, MODE_DECLARATION, MODE_DECLARATION, COMMA_SYMBOL, DEFINING_INDICANT, EQUALS_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, PROCEDURE_VARIABLE_DECLARATION, PROCEDURE_VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, BRIEF_OPERATOR_DECLARATION, BRIEF_OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, ROUTINE_TEXT, NULL_ATTRIBUTE);
/* Errors. WILDCARD catches TERTIARY which catches IDENTIFIER. */
      try_reduction (q, strange_tokens, &siga, PROCEDURE_DECLARATION, PROCEDURE_DECLARATION, COMMA_SYMBOL, WILDCARD, ROUTINE_TEXT, NULL_ATTRIBUTE);
    } while (siga);
  }
}

/*!
\brief reduce units
\param p position in tree
**/

static void reduce_units (NODE_T * p)
{
  NODE_T *q;
/* Stray ~ is a SKIP. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, OPERATOR) && WHETHER_LITERALLY (q, "~")) {
      ATTRIBUTE (q) = SKIP;
    }
  }
/* Reduce units. */
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, UNIT, ASSIGNATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, IDENTITY_RELATION, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, UNIT, AND_FUNCTION, NULL_ATTRIBUTE);
    try_reduction (q, a68_extension, NULL, UNIT, OR_FUNCTION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, ROUTINE_TEXT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, JUMP, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, SKIP, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, TERTIARY, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, UNIT, ASSERTION, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce_generic arguments
\param p position in tree
**/

static void reduce_generic_arguments (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, UNIT)) {
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, COLON_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, UNIT, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
    } else if (WHETHER (q, COLON_SYMBOL)) {
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, COLON_SYMBOL, NULL_ATTRIBUTE);
    } else if (WHETHER (q, DOTDOT_SYMBOL)) {
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, NULL, TRIMMER, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
    }
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, TRIMMER, UNIT, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, TRIMMER, AT_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
  for (q = p; q && NEXT (q); FORWARD (q)) {
    if (WHETHER (q, COMMA_SYMBOL)) {
      if (!(ATTRIBUTE (NEXT (q)) == UNIT || ATTRIBUTE (NEXT (q)) == TRIMMER)) {
        pad_node (q, TRIMMER);
      }
    } else {
      if (WHETHER (NEXT (q), COMMA_SYMBOL)) {
        if (WHETHER_NOT (q, UNIT) && WHETHER_NOT (q, TRIMMER)) {
          pad_node (q, TRIMMER);
        }
      }
    }
  }
  q = NEXT (p);
  ABEND (q == NULL, "erroneous parser state", NULL);
  try_reduction (q, NULL, NULL, GENERIC_ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, GENERIC_ARGUMENT_LIST, TRIMMER, NULL_ATTRIBUTE);
  do {
    siga = A68_FALSE;
    try_reduction (q, NULL, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, COMMA_SYMBOL, TRIMMER, NULL_ATTRIBUTE);
    try_reduction (q, strange_separator, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, strange_separator, &siga, GENERIC_ARGUMENT_LIST, GENERIC_ARGUMENT_LIST, TRIMMER, NULL_ATTRIBUTE);
  } while (siga);
}

/*!
\brief reduce bounds
\param p position in tree
**/

static void reduce_bounds (NODE_T * p)
{
  NODE_T *q;
  BOOL_T siga;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, BOUND, UNIT, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, BOUND, UNIT, DOTDOT_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, BOUND, UNIT, NULL_ATTRIBUTE);
  }
  q = NEXT (p);
  try_reduction (q, NULL, NULL, BOUNDS_LIST, BOUND, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, ALT_FORMAL_BOUNDS_LIST, COLON_SYMBOL, NULL_ATTRIBUTE);
  try_reduction (q, NULL, NULL, ALT_FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
  do {
    siga = A68_FALSE;
    try_reduction (q, NULL, &siga, BOUNDS_LIST, BOUNDS_LIST, COMMA_SYMBOL, BOUND, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COMMA_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, COLON_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, ALT_FORMAL_BOUNDS_LIST, FORMAL_BOUNDS_LIST, DOTDOT_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, NULL, &siga, FORMAL_BOUNDS_LIST, ALT_FORMAL_BOUNDS_LIST, COMMA_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (q, strange_separator, &siga, BOUNDS_LIST, BOUNDS_LIST, BOUND, NULL_ATTRIBUTE);
  } while (siga);
}

/*!
\brief reduce argument packs
\param p position in tree
**/

static void reduce_arguments (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    try_reduction (q, NULL, NULL, ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, ARGUMENT_LIST, ARGUMENT_LIST, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, strange_separator, &siga, ARGUMENT_LIST, ARGUMENT_LIST, UNIT, NULL_ATTRIBUTE);
    } while (siga);
  }
}

/*!
\brief reduce declaration lists
\param p position in tree
**/

static void reduce_declaration_lists (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, IDENTITY_DECLARATION, DECLARER, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, QUALIFIER, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, VARIABLE_DECLARATION, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, IDENTITY_DECLARATION, IDENTITY_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
      try_reduction (q, NULL, &siga, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE);
      if (!whether (q, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, UNIT, NULL_ATTRIBUTE)) {
        try_reduction (q, NULL, &siga, VARIABLE_DECLARATION, VARIABLE_DECLARATION, COMMA_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
      }
    } while (siga);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, OPERATOR_DECLARATION, OPERATOR_PLAN, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, OPERATOR_DECLARATION, OPERATOR_DECLARATION, COMMA_SYMBOL, DEFINING_OPERATOR, EQUALS_SYMBOL, UNIT, NULL_ATTRIBUTE);
    } while (siga);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, DECLARATION_LIST, MODE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, PRIORITY_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, BRIEF_OPERATOR_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, OPERATOR_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, IDENTITY_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, PROCEDURE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, PROCEDURE_VARIABLE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, VARIABLE_DECLARATION, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, DECLARATION_LIST, ENVIRON_NAME, NULL_ATTRIBUTE);
  }
  for (q = p; q != NULL; FORWARD (q)) {
    BOOL_T siga;
    do {
      siga = A68_FALSE;
      try_reduction (q, NULL, &siga, DECLARATION_LIST, DECLARATION_LIST, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
    } while (siga);
  }
}

/*!
\brief reduce labels and specifiers
\param p position in tree
**/

static void reduce_labels (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    try_reduction (q, NULL, NULL, LABELED_UNIT, LABEL, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SPECIFIED_UNIT, SPECIFIER, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
  }
}

/*!
\brief signal badly used exits
\param p position in tree
**/

static void precheck_serial_clause (NODE_T * q)
{
  NODE_T *p;
  BOOL_T label_seen = A68_FALSE;
/* Wrong exits. */
  for (p = q; p != NULL; FORWARD (p)) {
    if (WHETHER (p, EXIT_SYMBOL)) {
      if (NEXT (p) == NULL || WHETHER_NOT (NEXT (p), LABELED_UNIT)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_LABELED_UNIT_MUST_FOLLOW);
      }
    }
  }
/* Wrong jumps and declarations. */
  for (p = q; p != NULL; FORWARD (p)) {
    if (WHETHER (p, LABELED_UNIT)) {
      label_seen = A68_TRUE;
    } else if (WHETHER (p, DECLARATION_LIST)) {
      if (label_seen) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_LABEL_BEFORE_DECLARATION);
      }
    }
  }
}

/*!
\brief reduce serial clauses
\param p position in tree
**/

static void reduce_serial_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    precheck_serial_clause (p);
    try_reduction (q, NULL, NULL, SERIAL_CLAUSE, LABELED_UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, SERIAL_CLAUSE, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
    do {
      siga = A68_FALSE;
      if (WHETHER (q, SERIAL_CLAUSE)) {
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, EXIT_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, SEMI_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        /* Errors */
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COMMA_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, COLON_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, SERIAL_CLAUSE, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, SERIAL_CLAUSE, DECLARATION_LIST, NULL_ATTRIBUTE);
      } else if (WHETHER (q, INITIALISER_SERIES)) {
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        /* Errors */
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SERIAL_CLAUSE, INITIALISER_SERIES, LABELED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
      }
    } 
    while (siga);
  }
}

/*!
\brief reduce enquiry clauses
\param p position in tree
**/

static void reduce_enquiry_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    BOOL_T siga;
    try_reduction (q, NULL, NULL, ENQUIRY_CLAUSE, UNIT, NULL_ATTRIBUTE);
    try_reduction (q, NULL, NULL, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
    do {
      siga = A68_FALSE;
      if (WHETHER (q, ENQUIRY_CLAUSE)) {
        try_reduction (q, NULL, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, ENQUIRY_CLAUSE, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, ENQUIRY_CLAUSE, DECLARATION_LIST, NULL_ATTRIBUTE);
      } else if (WHETHER (q, INITIALISER_SERIES)) {
        try_reduction (q, NULL, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, SEMI_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, NULL, &siga, INITIALISER_SERIES, INITIALISER_SERIES, SEMI_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COMMA_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, COLON_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, COLON_SYMBOL, DECLARATION_LIST, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, ENQUIRY_CLAUSE, INITIALISER_SERIES, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, INITIALISER_SERIES, INITIALISER_SERIES, DECLARATION_LIST, NULL_ATTRIBUTE);
      }
    } 
    while (siga);
  }
}

/*!
\brief reduce collateral clauses
\param p position in tree
**/

static void reduce_collateral_clauses (NODE_T * p)
{
  if (NEXT (p) != NULL) {
    NODE_T *q = NEXT (p);
    if (WHETHER (q, UNIT)) {
      BOOL_T siga;
      try_reduction (q, NULL, NULL, UNIT_LIST, UNIT, NULL_ATTRIBUTE);
      do {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, UNIT_LIST, UNIT_LIST, COMMA_SYMBOL, UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, UNIT_LIST, UNIT_LIST, UNIT, NULL_ATTRIBUTE);
      } while (siga);
    } else if (WHETHER (q, SPECIFIED_UNIT)) {
      BOOL_T siga;
      try_reduction (q, NULL, NULL, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE);
      do {
        siga = A68_FALSE;
        try_reduction (q, NULL, &siga, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, COMMA_SYMBOL, SPECIFIED_UNIT, NULL_ATTRIBUTE);
        try_reduction (q, strange_separator, &siga, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT_LIST, SPECIFIED_UNIT, NULL_ATTRIBUTE);
      } while (siga);
    }
  }
}

/*!
\brief reduces clause parts, before reducing the clause itself
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void reduce_enclosed_clause_bits (NODE_T * p, int expect)
{
  if (SUB (p) != NULL) {
    return;
  }
  if (WHETHER (p, FOR_SYMBOL)) {
    try_reduction (p, NULL, NULL, FOR_PART, FOR_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OPEN_SYMBOL)) {
    if (expect == ENQUIRY_CLAUSE) {
      try_reduction (p, NULL, NULL, OPEN_PART, OPEN_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    } else if (expect == ARGUMENT) {
      try_reduction (p, NULL, NULL, ARGUMENT, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, ARGUMENT, OPEN_SYMBOL, ARGUMENT_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, empty_clause, NULL, ARGUMENT, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    } else if (expect == GENERIC_ARGUMENT) {
      if (whether (p, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE)) {
        pad_node (p, TRIMMER);
        try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, OPEN_SYMBOL, TRIMMER, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      }
      try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, OPEN_SYMBOL, GENERIC_ARGUMENT_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    } else if (expect == BOUNDS) {
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, BOUNDS, OPEN_SYMBOL, BOUNDS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, OPEN_SYMBOL, ALT_FORMAL_BOUNDS_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    } else {
      try_reduction (p, NULL, NULL, CLOSED_CLAUSE, OPEN_SYMBOL, SERIAL_CLAUSE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, OPEN_SYMBOL, UNIT_LIST, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, OPEN_SYMBOL, CLOSE_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, empty_clause, NULL, CLOSED_CLAUSE, OPEN_SYMBOL, INITIALISER_SERIES, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    }
  } else if (WHETHER (p, SUB_SYMBOL)) {
    if (expect == GENERIC_ARGUMENT) {
      if (whether (p, SUB_SYMBOL, BUS_SYMBOL, NULL_ATTRIBUTE)) {
        pad_node (p, TRIMMER);
        try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, SUB_SYMBOL, TRIMMER, BUS_SYMBOL, NULL_ATTRIBUTE);
      }
      try_reduction (p, NULL, NULL, GENERIC_ARGUMENT, SUB_SYMBOL, GENERIC_ARGUMENT_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
    } else if (expect == BOUNDS) {
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, BUS_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, BOUNDS, SUB_SYMBOL, BOUNDS_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, FORMAL_BOUNDS_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
      try_reduction (p, NULL, NULL, FORMAL_BOUNDS, SUB_SYMBOL, ALT_FORMAL_BOUNDS_LIST, BUS_SYMBOL, NULL_ATTRIBUTE);
    }
  } else if (WHETHER (p, BEGIN_SYMBOL)) {
    try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, BEGIN_SYMBOL, UNIT_LIST, END_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, COLLATERAL_CLAUSE, BEGIN_SYMBOL, END_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CLOSED_CLAUSE, BEGIN_SYMBOL, SERIAL_CLAUSE, END_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, CLOSED_CLAUSE, BEGIN_SYMBOL, INITIALISER_SERIES, END_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL)) {
    try_reduction (p, NULL, NULL, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, PICTURE_LIST, FORMAT_DELIMITER_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, FORMAT_TEXT, FORMAT_DELIMITER_SYMBOL, FORMAT_DELIMITER_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FORMAT_OPEN_SYMBOL)) {
    try_reduction (p, NULL, NULL, COLLECTION, FORMAT_OPEN_SYMBOL, PICTURE_LIST, FORMAT_CLOSE_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, CODE_SYMBOL)) {
    try_reduction (p, NULL, NULL, CODE_CLAUSE, CODE_SYMBOL, SERIAL_CLAUSE, EDOC_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, IF_SYMBOL)) {
    try_reduction (p, NULL, NULL, IF_PART, IF_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, IF_PART, IF_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, THEN_SYMBOL)) {
    try_reduction (p, NULL, NULL, THEN_PART, THEN_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, THEN_PART, THEN_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELSE_SYMBOL)) {
    try_reduction (p, NULL, NULL, ELSE_PART, ELSE_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, ELSE_PART, ELSE_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELIF_SYMBOL)) {
    try_reduction (p, NULL, NULL, ELIF_IF_PART, ELIF_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
  } else if (WHETHER (p, CASE_SYMBOL)) {
    try_reduction (p, NULL, NULL, CASE_PART, CASE_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, CASE_PART, CASE_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, IN_SYMBOL)) {
    try_reduction (p, NULL, NULL, INTEGER_IN_PART, IN_SYMBOL, UNIT_LIST, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_IN_PART, IN_SYMBOL, SPECIFIED_UNIT_LIST, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OUT_SYMBOL)) {
    try_reduction (p, NULL, NULL, OUT_PART, OUT_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, OUT_PART, OUT_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OUSE_SYMBOL)) {
    try_reduction (p, NULL, NULL, OUSE_CASE_PART, OUSE_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
  } else if (WHETHER (p, THEN_BAR_SYMBOL)) {
    try_reduction (p, NULL, NULL, CHOICE, THEN_BAR_SYMBOL, SERIAL_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CHOICE_CLAUSE, THEN_BAR_SYMBOL, UNIT_LIST, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT_LIST, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CHOICE, THEN_BAR_SYMBOL, SPECIFIED_UNIT, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, CHOICE, THEN_BAR_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELSE_BAR_SYMBOL)) {
    try_reduction (p, NULL, NULL, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, ELSE_OPEN_PART, ELSE_BAR_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FROM_SYMBOL)) {
    try_reduction (p, NULL, NULL, FROM_PART, FROM_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, BY_SYMBOL)) {
    try_reduction (p, NULL, NULL, BY_PART, BY_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, TO_SYMBOL)) {
    try_reduction (p, NULL, NULL, TO_PART, TO_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, DOWNTO_SYMBOL)) {
    try_reduction (p, NULL, NULL, TO_PART, DOWNTO_SYMBOL, UNIT, NULL_ATTRIBUTE);
  } else if (WHETHER (p, WHILE_SYMBOL)) {
    try_reduction (p, NULL, NULL, WHILE_PART, WHILE_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, WHILE_PART, WHILE_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, UNTIL_SYMBOL)) {
    try_reduction (p, NULL, NULL, UNTIL_PART, UNTIL_SYMBOL, ENQUIRY_CLAUSE, NULL_ATTRIBUTE);
    try_reduction (p, empty_clause, NULL, UNTIL_PART, UNTIL_SYMBOL, INITIALISER_SERIES, NULL_ATTRIBUTE);
  } else if (WHETHER (p, DO_SYMBOL)) {
    try_reduction (p, NULL, NULL, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, DO_PART, DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, DO_PART, DO_SYMBOL, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ALT_DO_SYMBOL)) {
    try_reduction (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, SERIAL_CLAUSE, OD_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ALT_DO_PART, ALT_DO_SYMBOL, UNTIL_PART, OD_SYMBOL, NULL_ATTRIBUTE);
  }
}

/*!
\brief reduce enclosed clauses
\param p position in tree
**/

static void reduce_enclosed_clauses (NODE_T * p)
{
  if (SUB (p) == NULL) {
    return;
  }
  if (WHETHER (p, OPEN_PART)) {
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, OPEN_PART, CHOICE, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, OPEN_PART, INTEGER_CHOICE_CLAUSE, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, OPEN_PART, UNITED_CHOICE, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELSE_OPEN_PART)) {
    try_reduction (p, NULL, NULL, BRIEF_ELIF_IF_PART, ELSE_OPEN_PART, CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_ELIF_IF_PART, ELSE_OPEN_PART, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_ELIF_IF_PART, ELSE_OPEN_PART, CHOICE, BRIEF_ELIF_IF_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_INTEGER_OUSE_PART, ELSE_OPEN_PART, INTEGER_CHOICE_CLAUSE, BRIEF_INTEGER_OUSE_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, CLOSE_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, BRIEF_UNITED_OUSE_PART, ELSE_OPEN_PART, UNITED_CHOICE, BRIEF_UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, IF_PART)) {
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, ELIF_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, CONDITIONAL_CLAUSE, IF_PART, THEN_PART, FI_SYMBOL, NULL_ATTRIBUTE);
  } else if (WHETHER (p, ELIF_IF_PART)) {
    try_reduction (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, ELSE_PART, FI_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, FI_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, ELIF_PART, ELIF_IF_PART, THEN_PART, ELIF_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, CASE_PART)) {
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_CASE_CLAUSE, CASE_PART, INTEGER_IN_PART, INTEGER_OUT_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_CASE_CLAUSE, CASE_PART, UNITED_IN_PART, UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, OUSE_CASE_PART)) {
    try_reduction (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, INTEGER_OUT_PART, OUSE_CASE_PART, INTEGER_IN_PART, INTEGER_OUT_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, OUT_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, ESAC_SYMBOL, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, UNITED_OUSE_PART, OUSE_CASE_PART, UNITED_IN_PART, UNITED_OUSE_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FOR_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, FROM_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FOR_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, FROM_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, FROM_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, BY_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, BY_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, TO_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, TO_PART, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, TO_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, WHILE_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, WHILE_PART, ALT_DO_PART, NULL_ATTRIBUTE);
  } else if (WHETHER (p, DO_PART)) {
    try_reduction (p, NULL, NULL, LOOP_CLAUSE, DO_PART, NULL_ATTRIBUTE);
  }
}

/*!
\brief substitute reduction when a phrase could not be parsed
\param p position in tree
\param expect information the parser may have on what is expected
\param suppress suppresses a diagnostic_node message (nested c.q. related diagnostics)
**/

static void recover_from_error (NODE_T * p, int expect, BOOL_T suppress)
{
/* This routine does not do fancy things as that might introduce more errors. */
  NODE_T *q = p;
  if (p == NULL) {
    return;
  }
  if (expect == SOME_CLAUSE) {
    expect = serial_or_collateral (p);
  }
  if (!suppress) {
/* Give an error message. */
    NODE_T *w = p;
    char *seq = phrase_to_text (p, &w);
    if (strlen (seq) == 0) {
      if (program.error_count == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, w, ERROR_SYNTAX_EXPECTED, expect, NULL);
      }
    } else {
      diagnostic_node (A68_SYNTAX_ERROR, w, ERROR_INVALID_SEQUENCE, seq, expect, NULL);
    }
    if (program.error_count >= MAX_ERRORS) {
      longjmp (bottom_up_crash_exit, 1);
    }
  }
/* Try to prevent spurious diagnostics by guessing what was expected. */
  while (NEXT (q) != NULL) {
    FORWARD (q);
  }
  if (whether_one_of (p, BEGIN_SYMBOL, OPEN_SYMBOL, NULL_ATTRIBUTE)) {
    if (expect == ARGUMENT || expect == COLLATERAL_CLAUSE || expect == PARAMETER_PACK || expect == STRUCTURE_PACK || expect == UNION_PACK) {
      make_sub (p, q, expect);
    } else if (expect == ENQUIRY_CLAUSE) {
      make_sub (p, q, OPEN_PART);
    } else if (expect == FORMAL_DECLARERS) {
      make_sub (p, q, FORMAL_DECLARERS);
    } else {
      make_sub (p, q, CLOSED_CLAUSE);
    }
  } else if (WHETHER (p, FORMAT_DELIMITER_SYMBOL) && expect == FORMAT_TEXT) {
    make_sub (p, q, FORMAT_TEXT);
  } else if (WHETHER (p, CODE_SYMBOL)) {
    make_sub (p, q, CODE_CLAUSE);
  } else if (whether_one_of (p, THEN_BAR_SYMBOL, CHOICE, NULL_ATTRIBUTE)) {
    make_sub (p, q, CHOICE);
  } else if (whether_one_of (p, IF_SYMBOL, IF_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, IF_PART);
  } else if (whether_one_of (p, THEN_SYMBOL, THEN_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, THEN_PART);
  } else if (whether_one_of (p, ELSE_SYMBOL, ELSE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, ELSE_PART);
  } else if (whether_one_of (p, ELIF_SYMBOL, ELIF_IF_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, ELIF_IF_PART);
  } else if (whether_one_of (p, CASE_SYMBOL, CASE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, CASE_PART);
  } else if (whether_one_of (p, OUT_SYMBOL, OUT_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, OUT_PART);
  } else if (whether_one_of (p, OUSE_SYMBOL, OUSE_CASE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, OUSE_CASE_PART);
  } else if (whether_one_of (p, FOR_SYMBOL, FOR_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, FOR_PART);
  } else if (whether_one_of (p, FROM_SYMBOL, FROM_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, FROM_PART);
  } else if (whether_one_of (p, BY_SYMBOL, BY_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, BY_PART);
  } else if (whether_one_of (p, TO_SYMBOL, DOWNTO_SYMBOL, TO_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, TO_PART);
  } else if (whether_one_of (p, WHILE_SYMBOL, WHILE_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, WHILE_PART);
  } else if (whether_one_of (p, UNTIL_SYMBOL, UNTIL_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, UNTIL_PART);
  } else if (whether_one_of (p, DO_SYMBOL, DO_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, DO_PART);
  } else if (whether_one_of (p, ALT_DO_SYMBOL, ALT_DO_PART, NULL_ATTRIBUTE)) {
    make_sub (p, q, ALT_DO_PART);
  } else if (non_terminal_string (edit_line, expect) != NULL) {
    make_sub (p, q, expect);
  }
}

/*!
\brief heuristic aid in pinpointing errors
\param p position in tree
**/

static void reduce_erroneous_units (NODE_T * p)
{
/* Constructs are reduced to units in an attempt to limit spurious diagnostics. */
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
/* Some implementations allow selection from a tertiary, when there is no risk
of ambiguity. Algol68G follows RR, so some extra attention here to guide an
unsuspecting user. */
    if (whether (q, SELECTOR, -SECONDARY, NULL_ATTRIBUTE)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, SECONDARY);
      try_reduction (q, NULL, NULL, UNIT, SELECTOR, WILDCARD, NULL_ATTRIBUTE);
    }
/* Attention for identity relations that require tertiaries. */
    if (whether (q, -TERTIARY, IS_SYMBOL, TERTIARY, NULL_ATTRIBUTE) || whether (q, TERTIARY, IS_SYMBOL, -TERTIARY, NULL_ATTRIBUTE) || whether (q, -TERTIARY, IS_SYMBOL, -TERTIARY, NULL_ATTRIBUTE)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      try_reduction (q, NULL, NULL, UNIT, WILDCARD, IS_SYMBOL, WILDCARD, NULL_ATTRIBUTE);
    } else if (whether (q, -TERTIARY, ISNT_SYMBOL, TERTIARY, NULL_ATTRIBUTE) || whether (q, TERTIARY, ISNT_SYMBOL, -TERTIARY, NULL_ATTRIBUTE) || whether (q, -TERTIARY, ISNT_SYMBOL, -TERTIARY, NULL_ATTRIBUTE)) {
      diagnostic_node (A68_SYNTAX_ERROR, NEXT (q), ERROR_SYNTAX_EXPECTED, TERTIARY);
      try_reduction (q, NULL, NULL, UNIT, WILDCARD, ISNT_SYMBOL, WILDCARD, NULL_ATTRIBUTE);
    }
  }
}

/*
Here is a set of routines that gather definitions from phrases.
This way we can apply tags before defining them.
These routines do not look very elegant as they have to scan through all
kind of symbols to find a pattern that they recognise.
*/

/*!
\brief skip anything until a comma, semicolon or EXIT is found
\param p position in tree
\return node from where to proceed
**/

static NODE_T *skip_unit (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, COMMA_SYMBOL)) {
      return (p);
    } else if (WHETHER (p, SEMI_SYMBOL)) {
      return (p);
    } else if (WHETHER (p, EXIT_SYMBOL)) {
      return (p);
    }
  }
  return (NULL);
}

/*!
\brief attribute of entry in symbol table
\param table current symbol table
\param name token name
\return attribute of entry in symbol table, or 0 if not found
**/

static int find_tag_definition (SYMBOL_TABLE_T * table, char *name)
{
  if (table != NULL) {
    int ret = 0;
    TAG_T *s;
    BOOL_T found;
    found = A68_FALSE;
    for (s = table->indicants; s != NULL && !found; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        ret += INDICANT;
        found = A68_TRUE;
      }
    }
    found = A68_FALSE;
    for (s = table->operators; s != NULL && !found; FORWARD (s)) {
      if (SYMBOL (NODE (s)) == name) {
        ret += OPERATOR;
        found = A68_TRUE;
      }
    }
    if (ret == 0) {
      return (find_tag_definition (PREVIOUS (table), name));
    } else {
      return (ret);
    }
  } else {
    return (0);
  }
}

/*!
\brief fill in whether bold tag is operator or indicant
\param p position in tree
**/

static void elaborate_bold_tags (NODE_T * p)
{
  NODE_T *q;
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, BOLD_TAG)) {
      switch (find_tag_definition (SYMBOL_TABLE (q), SYMBOL (q))) {
      case 0:
        {
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
          break;
        }
      case INDICANT:
        {
          ATTRIBUTE (q) = INDICANT;
          break;
        }
      case OPERATOR:
        {
          ATTRIBUTE (q) = OPERATOR;
          break;
        }
      }
    }
  }
}

/*!
\brief skip declarer, or argument pack and declarer
\param p position in tree
\return node before token that is not part of pack or declarer 
**/

static NODE_T *skip_pack_declarer (NODE_T * p)
{
/* Skip () REF [] REF FLEX [] [] ... */
  while (p != NULL && (whether_one_of (p, SUB_SYMBOL, OPEN_SYMBOL, REF_SYMBOL, FLEX_SYMBOL, SHORT_SYMBOL, LONG_SYMBOL, NULL_ATTRIBUTE))) {
    FORWARD (p);
  }
/* Skip STRUCT (), UNION () or PROC [()]. */
  if (p != NULL && (whether_one_of (p, STRUCT_SYMBOL, UNION_SYMBOL, NULL_ATTRIBUTE))) {
    return (NEXT (p));
  } else if (p != NULL && WHETHER (p, PROC_SYMBOL)) {
    return (skip_pack_declarer (NEXT (p)));
  } else {
    return (p);
  }
}

/*!
\brief search MODE A = .., B = .. and store indicants
\param p position in tree
**/

static void extract_indicants (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER (q, MODE_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, MODE_DECLARATION);
        if (whether (q, BOLD_TAG, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
          ASSERT (add_tag (SYMBOL_TABLE (p), INDICANT, q, NULL, NULL_ATTRIBUTE) != NULL);
          ATTRIBUTE (q) = DEFINING_INDICANT;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_pack_declarer (NEXT (q));
          FORWARD (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

#define GET_PRIORITY(q, k)\
  RESET_ERRNO;\
  (k) = atoi (SYMBOL (q));\
  if (errno != 0) {\
    diagnostic_node (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY, NULL);\
    (k) = MAX_PRIORITY;\
  } else if ((k) < 1 || (k) > MAX_PRIORITY) {\
    diagnostic_node (A68_SYNTAX_ERROR, (q), ERROR_INVALID_PRIORITY, NULL);\
    (k) = MAX_PRIORITY;\
  }

/*!
\brief search PRIO X = .., Y = .. and store priorities
\param p position in tree
**/

static void extract_priorities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER (q, PRIO_SYMBOL)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        detect_redefined_keyword (q, PRIORITY_DECLARATION);
/* An operator tag like ++ or && gives strange errors so we catch it here. */
        if (whether (q, OPERATOR, OPERATOR, NULL_ATTRIBUTE)) {
          int k;
          NODE_T *y = q;
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG, NULL);
          ATTRIBUTE (q) = DEFINING_OPERATOR;
/* Remove one superfluous operator, and hope it was only one.   	 */
          NEXT (q) = NEXT_NEXT (q);
          PREVIOUS (NEXT (q)) = q;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
          FORWARD (q);
        } else if (whether (q, OPERATOR, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE) || 
                   whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, IDENTIFIER, NULL_ATTRIBUTE)) {
          siga = A68_FALSE;
        } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE)) {
          int k;
          NODE_T *y = q;
          ATTRIBUTE (q) = DEFINING_OPERATOR;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          FORWARD (q);
          GET_PRIORITY (q, k);
          ATTRIBUTE (q) = PRIORITY;
          ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
          FORWARD (q);
        } else if (whether (q, BOLD_TAG, INT_DENOTATION, NULL_ATTRIBUTE) || 
                   whether (q, OPERATOR, INT_DENOTATION, NULL_ATTRIBUTE) || 
                   whether (q, EQUALS_SYMBOL, INT_DENOTATION, NULL_ATTRIBUTE)) {
/* The scanner cannot separate operator and "=" sign so we do this here. */
          int len = (int) strlen (SYMBOL (q));
          if (len > 1 && SYMBOL (q)[len - 1] == '=') {
            int k;
            NODE_T *y = q;
            char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
            bufcpy (sym, SYMBOL (q), len + 1);
            sym[len - 1] = NULL_CHAR;
            SYMBOL (q) = TEXT (add_token (&top_token, sym));
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            insert_node (q, ALT_EQUALS_SYMBOL);
            q = NEXT_NEXT (q);
            GET_PRIORITY (q, k);
            ATTRIBUTE (q) = PRIORITY;
            ASSERT (add_tag (SYMBOL_TABLE (p), PRIO_SYMBOL, y, NULL, k) != NULL);
            FORWARD (q);
          } else {
            siga = A68_FALSE;
          }
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search OP [( .. ) ..] X = .., Y = .. and store operators
\param p position in tree
**/

static void extract_operators (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (WHETHER_NOT (q, OP_SYMBOL)) {
      FORWARD (q);
    } else {
      BOOL_T siga = A68_TRUE;
/* Skip operator plan. */
      if (NEXT (q) != NULL && WHETHER (NEXT (q), OPEN_SYMBOL)) {
        q = skip_pack_declarer (NEXT (q));
      }
/* Sample operators. */
      if (q != NULL) {
        do {
          FORWARD (q);
          detect_redefined_keyword (q, OPERATOR_DECLARATION);
/* An unacceptable operator tag like ++ or && gives strange errors so we catch it here. */
          if (whether (q, OPERATOR, OPERATOR, NULL_ATTRIBUTE)) {
            diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_INVALID_OPERATOR_TAG, NULL);
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
            NEXT (q) = NEXT_NEXT (q);   /* Remove one superfluous operator, and hope it was only one. */
            PREVIOUS (NEXT (q)) = q;
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, OPERATOR, EQUALS_SYMBOL, NULL_ATTRIBUTE) || 
                     whether (q, EQUALS_SYMBOL, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (whether (q, BOLD_TAG, IDENTIFIER, NULL_ATTRIBUTE)) {
            siga = A68_FALSE;
          } else if (whether (q, BOLD_TAG, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
            ATTRIBUTE (q) = DEFINING_OPERATOR;
            ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
            FORWARD (q);
            ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
            q = skip_unit (q);
          } else if (q != NULL && (whether_one_of (q, OPERATOR, BOLD_TAG, EQUALS_SYMBOL, NULL_ATTRIBUTE))) {
/* The scanner cannot separate operator and "=" sign so we do this here. */
            int len = (int) strlen (SYMBOL (q));
            if (len > 1 && SYMBOL (q)[len - 1] == '=') {
              char *sym = (char *) get_temp_heap_space ((size_t) (len + 1));
              bufcpy (sym, SYMBOL (q), len + 1);
              sym[len - 1] = NULL_CHAR;
              SYMBOL (q) = TEXT (add_token (&top_token, sym));
              ATTRIBUTE (q) = DEFINING_OPERATOR;
              insert_node (q, ALT_EQUALS_SYMBOL);
              ASSERT (add_tag (SYMBOL_TABLE (p), OP_SYMBOL, q, NULL, NULL_ATTRIBUTE) != NULL);
              FORWARD (q);
              q = skip_unit (q);
            } else {
              siga = A68_FALSE;
            }
          } else {
            siga = A68_FALSE;
          }
        } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
      }
    }
  }
}

/*!
\brief search and store labels
\param p position in tree
\param expect information the parser may have on what is expected
**/

static void extract_labels (NODE_T * p, int expect)
{
  NODE_T *q;
/* Only handle candidate phrases as not to search indexers! */
  if (expect == SERIAL_CLAUSE || expect == ENQUIRY_CLAUSE || expect == SOME_CLAUSE) {
    for (q = p; q != NULL; FORWARD (q)) {
      if (whether (q, IDENTIFIER, COLON_SYMBOL, NULL_ATTRIBUTE)) {
        TAG_T *z = add_tag (SYMBOL_TABLE (p), LABEL, q, NULL, LOCAL_LABEL);
        ATTRIBUTE (q) = DEFINING_IDENTIFIER;
        z->unit = NULL;
      }
    }
  }
}

/*!
\brief search MOID x = .., y = .. and store identifiers
\param p position in tree
**/

static void extract_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, DECLARER, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        if (whether ((FORWARD (q)), IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          FORWARD (q);
          ATTRIBUTE (q) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs. */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search MOID x [:= ..], y [:= ..] and store identifiers
\param p position in tree
**/

static void extract_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, DECLARER, IDENTIFIER, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, NULL_ATTRIBUTE)) {
          if (whether (q, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs. */
            diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
            ATTRIBUTE (NEXT (q)) = ASSIGN_SYMBOL;
          }
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search PROC x = .., y = .. and stores identifiers
\param p position in tree
**/

static void extract_proc_identities (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
          TAG_T *t = add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER);
          t->in_proc = A68_TRUE;
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs. */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ALT_EQUALS_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief search PROC x [:= ..], y [:= ..]; store identifiers
\param p position in tree
**/

static void extract_proc_variables (NODE_T * p)
{
  NODE_T *q = p;
  while (q != NULL) {
    if (whether (q, PROC_SYMBOL, IDENTIFIER, NULL_ATTRIBUTE)) {
      BOOL_T siga = A68_TRUE;
      do {
        FORWARD (q);
        if (whether (q, IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)) {
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          q = skip_unit (FORWARD (q));
        } else if (whether (q, IDENTIFIER, EQUALS_SYMBOL, NULL_ATTRIBUTE)) {
/* Handle common error in ALGOL 68 programs. */
          diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_SYNTAX_MIXED_DECLARATION);
          ASSERT (add_tag (SYMBOL_TABLE (p), IDENTIFIER, q, NULL, NORMAL_IDENTIFIER) != NULL);
          ATTRIBUTE (q) = DEFINING_IDENTIFIER;
          ATTRIBUTE (FORWARD (q)) = ASSIGN_SYMBOL;
          q = skip_unit (q);
        } else {
          siga = A68_FALSE;
        }
      } while (siga && q != NULL && WHETHER (q, COMMA_SYMBOL));
    } else {
      FORWARD (q);
    }
  }
}

/*!
\brief schedule gathering of definitions in a phrase
\param p position in tree
**/

static void extract_declarations (NODE_T * p)
{
  NODE_T *q;
/* Get definitions so we know what is defined in this range. */
  extract_identities (p);
  extract_variables (p);
  extract_proc_identities (p);
  extract_proc_variables (p);
/* By now we know whether "=" is an operator or not. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = OPERATOR;
    } else if (WHETHER (q, ALT_EQUALS_SYMBOL)) {
      ATTRIBUTE (q) = EQUALS_SYMBOL;
    }
  }
/* Get qualifiers. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (whether (q, LOC_SYMBOL, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, DECLARER, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, LOC_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, HEAP_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
    if (whether (q, NEW_SYMBOL, PROC_SYMBOL, DEFINING_IDENTIFIER, NULL_ATTRIBUTE)) {
      make_sub (q, q, QUALIFIER);
    }
  }
/* Give priorities to operators. */
  for (q = p; q != NULL; FORWARD (q)) {
    if (WHETHER (q, OPERATOR)) {
      if (find_tag_global (SYMBOL_TABLE (q), OP_SYMBOL, SYMBOL (q))) {
        TAG_T *s = find_tag_global (SYMBOL_TABLE (q), PRIO_SYMBOL, SYMBOL (q));
        if (s != NULL) {
          PRIO (INFO (q)) = PRIO (s);
        } else {
          PRIO (INFO (q)) = 0;
        }
      } else {
        diagnostic_node (A68_SYNTAX_ERROR, q, ERROR_UNDECLARED_TAG);
        PRIO (INFO (q)) = 1;
      }
    }
  }
}

/* A posteriori checks of the syntax tree built by the BU parser. */

/*!
\brief count pictures
\param p position in tree
\param k counter
**/

static void count_pictures (NODE_T * p, int *k)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, PICTURE)) {
      (*k)++;
    }
    count_pictures (SUB (p), k);
  }
}

/*!
\brief driver for a posteriori error checking
\param p position in tree
**/

void bottom_up_error_check (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, BOOLEAN_PATTERN)) {
      int k = 0;
      count_pictures (SUB (p), &k);
      if (!(k == 0 || k == 2)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_FORMAT_PICTURE_NUMBER, ATTRIBUTE (p), NULL);
      }
    } else {
      bottom_up_error_check (SUB (p));
    }
  }
}

/* Next part rearranges the tree after the symbol tables are finished. */

/*!
\brief transfer IDENTIFIER to JUMP where appropriate
\param p position in tree
**/

void rearrange_goto_less_jumps (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, UNIT)) {
      NODE_T *q = SUB (p);
      if (WHETHER (q, TERTIARY)) {
        NODE_T *tertiary = q;
        q = SUB (q);
        if (q != NULL && WHETHER (q, SECONDARY)) {
          q = SUB (q);
          if (q != NULL && WHETHER (q, PRIMARY)) {
            q = SUB (q);
            if (q != NULL && WHETHER (q, IDENTIFIER)) {
              if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
                ATTRIBUTE (tertiary) = JUMP;
                SUB (tertiary) = q;
              }
            }
          }
        }
      }
    } else if (WHETHER (p, TERTIARY)) {
      NODE_T *q = SUB (p);
      if (q != NULL && WHETHER (q, SECONDARY)) {
        NODE_T *secondary = q;
        q = SUB (q);
        if (q != NULL && WHETHER (q, PRIMARY)) {
          q = SUB (q);
          if (q != NULL && WHETHER (q, IDENTIFIER)) {
            if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
              ATTRIBUTE (secondary) = JUMP;
              SUB (secondary) = q;
            }
          }
        }
      }
    } else if (WHETHER (p, SECONDARY)) {
      NODE_T *q = SUB (p);
      if (q != NULL && WHETHER (q, PRIMARY)) {
        NODE_T *primary = q;
        q = SUB (q);
        if (q != NULL && WHETHER (q, IDENTIFIER)) {
          if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
            ATTRIBUTE (primary) = JUMP;
            SUB (primary) = q;
          }
        }
      }
    } else if (WHETHER (p, PRIMARY)) {
      NODE_T *q = SUB (p);
      if (q != NULL && WHETHER (q, IDENTIFIER)) {
        if (whether_identifier_or_label_global (SYMBOL_TABLE (q), SYMBOL (q)) == LABEL) {
          make_sub (q, q, JUMP);
        }
      }
    }
    rearrange_goto_less_jumps (SUB (p));
  }
}

/* VICTAL CHECKER. Checks use of formal, actual and virtual declarers. */

/*!
\brief check generator
\param p position in tree
**/

static void victal_check_generator (NODE_T * p)
{
  if (!victal_check_declarer (NEXT (p), ACTUAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
  }
}

/*!
\brief check formal pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_formal_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, FORMAL_DECLARERS)) {
      victal_check_formal_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_formal_pack (NEXT (p), x, z);
    } else if (WHETHER (p, FORMAL_DECLARERS_LIST)) {
      victal_check_formal_pack (NEXT (p), x, z);
      victal_check_formal_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      victal_check_formal_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check operator declaration
\param p position in tree
**/

static void victal_check_operator_dec (NODE_T * p)
{
  if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
    BOOL_T z = A68_TRUE;
    victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
}

/*!
\brief check mode declaration
\param p position in tree
**/

static void victal_check_mode_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
      victal_check_mode_dec (NEXT (p));
    } else if (whether_one_of (p, MODE_SYMBOL, DEFINING_INDICANT, NULL_ATTRIBUTE)
               || whether_one_of (p, EQUALS_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_mode_dec (NEXT (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
    }
  }
}

/*!
\brief check variable declaration
\param p position in tree
**/

static void victal_check_variable_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
      victal_check_variable_dec (NEXT (p));
    } else if (whether_one_of (p, DEFINING_IDENTIFIER, ASSIGN_SYMBOL, NULL_ATTRIBUTE)
               || WHETHER (p, COMMA_SYMBOL)) {
      victal_check_variable_dec (NEXT (p));
    } else if (WHETHER (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, ACTUAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual declarer");
      }
      victal_check_variable_dec (NEXT (p));
    }
  }
}

/*!
\brief check identity declaration
\param p position in tree
**/

static void victal_check_identity_dec (NODE_T * p)
{
  if (p != NULL) {
    if (WHETHER (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
      victal_check_identity_dec (NEXT (p));
    } else if (whether_one_of (p, DEFINING_IDENTIFIER, EQUALS_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_identity_dec (NEXT (p));
    } else if (WHETHER (p, UNIT)) {
      victal_checker (SUB (p));
    } else if (WHETHER (p, DECLARER)) {
      if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      victal_check_identity_dec (NEXT (p));
    }
  }
}

/*!
\brief check routine pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_routine_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, PARAMETER_PACK)) {
      victal_check_routine_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_routine_pack (NEXT (p), x, z);
    } else if (whether_one_of (p, PARAMETER_LIST, PARAMETER, NULL_ATTRIBUTE)) {
      victal_check_routine_pack (NEXT (p), x, z);
      victal_check_routine_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      *z &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check routine text
\param p position in tree
**/

static void victal_check_routine_text (NODE_T * p)
{
  if (WHETHER (p, PARAMETER_PACK)) {
    BOOL_T z = A68_TRUE;
    victal_check_routine_pack (p, FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarers");
    }
    FORWARD (p);
  }
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
  }
  victal_checker (NEXT (p));
}

/*!
\brief check structure pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_structure_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, STRUCTURE_PACK)) {
      victal_check_structure_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_structure_pack (NEXT (p), x, z);
    } else if (whether_one_of (p, STRUCTURED_FIELD_LIST, STRUCTURED_FIELD, NULL_ATTRIBUTE)) {
      victal_check_structure_pack (NEXT (p), x, z);
      victal_check_structure_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      (*z) &= victal_check_declarer (SUB (p), x);
    }
  }
}

/*!
\brief check union pack
\param p position in tree
\param x expected attribute
\param z flag
**/

static void victal_check_union_pack (NODE_T * p, int x, BOOL_T * z)
{
  if (p != NULL) {
    if (WHETHER (p, UNION_PACK)) {
      victal_check_union_pack (SUB (p), x, z);
    } else if (whether_one_of (p, OPEN_SYMBOL, COMMA_SYMBOL, VOID_SYMBOL, NULL_ATTRIBUTE)) {
      victal_check_union_pack (NEXT (p), x, z);
    } else if (WHETHER (p, UNION_DECLARER_LIST)) {
      victal_check_union_pack (NEXT (p), x, z);
      victal_check_union_pack (SUB (p), x, z);
    } else if (WHETHER (p, DECLARER)) {
      victal_check_union_pack (NEXT (p), x, z);
      (*z) &= victal_check_declarer (SUB (p), FORMAL_DECLARER_MARK);
    }
  }
}

/*!
\brief check declarer
\param p position in tree
\param x expected attribute
**/

static BOOL_T victal_check_declarer (NODE_T * p, int x)
{
  if (p == NULL) {
    return (A68_FALSE);
  } else if (WHETHER (p, DECLARER)) {
    return (victal_check_declarer (SUB (p), x));
  } else if (whether_one_of (p, LONGETY, SHORTETY, NULL_ATTRIBUTE)) {
    return (A68_TRUE);
  } else if (whether_one_of (p, VOID_SYMBOL, INDICANT, STANDARD, NULL_ATTRIBUTE)) {
    return (A68_TRUE);
  } else if (WHETHER (p, REF_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), VIRTUAL_DECLARER_MARK));
  } else if (WHETHER (p, FLEX_SYMBOL)) {
    return (victal_check_declarer (NEXT (p), x));
  } else if (WHETHER (p, BOUNDS)) {
    victal_checker (SUB (p));
    if (x == FORMAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else if (x == VIRTUAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "virtual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (WHETHER (p, FORMAL_BOUNDS)) {
    victal_checker (SUB (p));
    if (x == ACTUAL_DECLARER_MARK) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "actual bounds");
      (void) victal_check_declarer (NEXT (p), x);
      return (A68_TRUE);
    } else {
      return (victal_check_declarer (NEXT (p), x));
    }
  } else if (WHETHER (p, STRUCT_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_structure_pack (NEXT (p), x, &z);
    return (z);
  } else if (WHETHER (p, UNION_SYMBOL)) {
    BOOL_T z = A68_TRUE;
    victal_check_union_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
    if (!z) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer pack");
    }
    return (A68_TRUE);
  } else if (WHETHER (p, PROC_SYMBOL)) {
    if (WHETHER (NEXT (p), FORMAL_DECLARERS)) {
      BOOL_T z = A68_TRUE;
      victal_check_formal_pack (NEXT (p), FORMAL_DECLARER_MARK, &z);
      if (!z) {
        diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
      }
      FORWARD (p);
    }
    if (!victal_check_declarer (NEXT (p), FORMAL_DECLARER_MARK)) {
      diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    }
    return (A68_TRUE);
  } else {
    return (A68_FALSE);
  }
}

/*!
\brief check cast
\param p position in tree
**/

static void victal_check_cast (NODE_T * p)
{
  if (!victal_check_declarer (p, FORMAL_DECLARER_MARK)) {
    diagnostic_node (A68_SYNTAX_ERROR, p, ERROR_EXPECTED, "formal declarer");
    victal_checker (NEXT (p));
  }
}

/*!
\brief driver for checking VICTALITY of declarers
\param p position in tree
**/

void victal_checker (NODE_T * p)
{
  for (; p != NULL; FORWARD (p)) {
    if (WHETHER (p, MODE_DECLARATION)) {
      victal_check_mode_dec (SUB (p));
    } else if (WHETHER (p, VARIABLE_DECLARATION)) {
      victal_check_variable_dec (SUB (p));
    } else if (WHETHER (p, IDENTITY_DECLARATION)) {
      victal_check_identity_dec (SUB (p));
    } else if (WHETHER (p, GENERATOR)) {
      victal_check_generator (SUB (p));
    } else if (WHETHER (p, ROUTINE_TEXT)) {
      victal_check_routine_text (SUB (p));
    } else if (WHETHER (p, OPERATOR_PLAN)) {
      victal_check_operator_dec (SUB (p));
    } else if (WHETHER (p, CAST)) {
      victal_check_cast (SUB (p));
    } else {
      victal_checker (SUB (p));
    }
  }
}

/*
\brief set level for procedures
\param p position in tree
\param n proc level number
*/

void set_proc_level (NODE_T * p, int n)
{
  for (; p != NULL; FORWARD (p)) {
    INFO (p)->PROCEDURE_LEVEL = n;
    if (WHETHER (p, ROUTINE_TEXT)) {
      set_proc_level (SUB (p), n + 1);
    } else {
      set_proc_level (SUB (p), n);
    }
  }
}

/*
\brief set nests for diagnostics
\param p position in tree
\param s start of enclosing nest
*/

void set_nest (NODE_T * p, NODE_T * s)
{
  for (; p != NULL; FORWARD (p)) {
    NEST (p) = s;
    if (WHETHER (p, PARTICULAR_PROGRAM)) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, CLOSED_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, COLLATERAL_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, CONDITIONAL_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, INTEGER_CASE_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, UNITED_CASE_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else if (WHETHER (p, LOOP_CLAUSE) && NUMBER (LINE (p)) != 0) {
      set_nest (SUB (p), p);
    } else {
      set_nest (SUB (p), s);
    }
  }
}
