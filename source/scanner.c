/*!
\file scanner.c
\brief tokenises source files as a linear list of tokens
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "algol68g.h"
#include "environ.h"
#include "genie.h"
#include "inline.h"

#define STOP_CHAR 127

#define IN_PRELUDE(p) (LINE_NUMBER (p) <= 0)
#define EOL(c) ((c) == NEWLINE_CHAR || (c) == NULL_CHAR)

void file_format_error (MODULE_T *, int);
static void append_source_line (MODULE_T *, char *, SOURCE_LINE_T **, int *, char *);

static char *scan_buf;
static int max_scan_buf_length, source_file_size;
static BOOL_T stop_scanner = A68_FALSE, read_error = A68_FALSE, no_preprocessing = A68_FALSE;

/*!
\brief save scanner state, for character look-ahead
\param module
\param ref_l source line
\param ref_s position in source line text
\param ch last scanned character
**/

static void save_state (MODULE_T * module, SOURCE_LINE_T * ref_l, char *ref_s, char ch)
{
  module->scan_state.save_l = ref_l;
  module->scan_state.save_s = ref_s;
  module->scan_state.save_c = ch;
}

/*!
\brief restore scanner state, for character look-ahead
\param module
\param ref_l source line
\param ref_s position in source line text
\param ch last scanned character
**/

static void restore_state (MODULE_T * module, SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  *ref_l = module->scan_state.save_l;
  *ref_s = module->scan_state.save_s;
  *ch = module->scan_state.save_c;
}

/*!
\brief whether ch is unworthy
\param u source line with error
\param v character in line
\param ch
**/

static void unworthy (SOURCE_LINE_T * u, char *v, char ch)
{
  snprintf (edit_line, BUFFER_SIZE, ERROR_UNWORTHY_CHARACTER, ctrl_char (ch));
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
    int len = strlen (z);
    if (len >= 2 && z[len - 2] == ESCAPE_CHAR && z[len - 1] == NEWLINE_CHAR && NEXT (q) != NULL && NEXT (q)->string != NULL) {
      z[len - 2] = NULL_CHAR;
      len += strlen (NEXT (q)->string);
      z = (char *) get_fixed_heap_space (len + 1);
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

static BOOL_T whether_bold (SOURCE_LINE_T * z, char *u, char *v)
{
  int len = strlen (v);
  if (MODULE (z)->options.stropping == QUOTE_STROPPING) {
    if (u[0] == '\'') {
      return (strncmp (++u, v, len) == 0 && u[len] == '\'');
    } else {
      return (A68_FALSE);
    }
  } else {
    return (strncmp (u, v, len) == 0 && !IS_UPPER (u[len]));
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
      if (whether_bold (u, v, "COMMENT") && delim == BOLD_COMMENT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (whether_bold (u, v, "CO") && delim == STYLE_I_COMMENT_SYMBOL) {
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
      if (whether_bold (u, v, "PRAGMAT") && delim == BOLD_PRAGMAT_SYMBOL) {
        *top = u;
        *ch = &v[1];
        return (A68_TRUE);
      } else if (whether_bold (u, v, "PR")
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
      else if (whether_bold (u, v, "COMMENT")) {
        SCAN_ERROR (!skip_comment (&u, &v, BOLD_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (whether_bold (u, v, "CO")) {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_I_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (v[0] == '#') {
        SCAN_ERROR (!skip_comment (&u, &v, STYLE_II_COMMENT_SYMBOL), start_l, start_c, ERROR_UNTERMINATED_COMMENT);
      } else if (whether_bold (u, v, "PRAGMAT")
                 || whether_bold (u, v, "PR")) {
/* We caught a PRAGMAT. */
        char *item;
        if (whether_bold (u, v, "PRAGMAT")) {
          *delim = BOLD_PRAGMAT_SYMBOL;
          v = &v[strlen ("PRAGMAT")];
        } else if (whether_bold (u, v, "PR")) {
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
        fnwid = strlen (MODULE (u)->files.path) + strlen (fnb) + 1;
        fn = (char *) get_fixed_heap_space (fnwid);
        bufcpy (fn, MODULE (u)->files.path, fnwid);
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
        fsize = lseek (fd, 0, SEEK_END);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        fbuf = (char *) get_temp_heap_space ((unsigned) (8 + fsize));
        RESET_ERRNO;
        lseek (fd, 0, SEEK_SET);
        SCAN_ERROR (errno != 0, start_l, start_c, ERROR_FILE_READ);
        RESET_ERRNO;
        bytes_read = io_read (fd, fbuf, fsize);
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
            SCAN_ERROR ((IS_CNTRL (fbuf[k]) && !IS_SPACE (fbuf[k]))
                        || fbuf[k] == STOP_CHAR, start_l, start_c, ERROR_FILE_INCLUDE_CTRL);
            scan_buf[n++] = fbuf[k++];
            scan_buf[n] = NULL_CHAR;
          }
          scan_buf[n++] = NEWLINE_CHAR;
          scan_buf[n] = NULL_CHAR;
          if (k < fsize) {
            k++;
          }
          append_source_line (MODULE (u), scan_buf, &t, &linum, fn);
        }
/* Conclude and go find another include directive, if any. */
        NEXT (t) = s;
        PREVIOUS (s) = t;
        concatenate_lines (top);
        close (fd);
        make_pass = A68_TRUE;
      }
    search_next_pragmat:       /* skip. */ ;
    }
  }
}

/*!
\brief append a source line to the internal source file
\param module module that reads source
\param str text line to be appended
\param ref_l previous source line
\param line_num previous source line number
\param filename name of file being read
**/

static void append_source_line (MODULE_T * module, char *str, SOURCE_LINE_T ** ref_l, int *line_num, char *filename)
{
  SOURCE_LINE_T *z = new_source_line ();
/* Allow shell command in first line, f.i. "#!/usr/share/bin/a68g". */
  if (*line_num == 1) {
    if (strlen (str) >= 2 && strncmp (str, "#!", 2) == 0) {
      (*line_num)++;
      return;
    }
  }
  if (module->options.reductions) {
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
  MODULE (z) = module;
  NEXT (z) = NULL;
  PREVIOUS (z) = *ref_l;
  if (module != NULL && module->top_line == NULL) {
    module->top_line = z;
  }
  if (*ref_l != NULL) {
    NEXT (*ref_l) = z;
  }
  *ref_l = z;
}

/*!
\brief size of source file
\param module module that reads source
\return size of file
**/

static int get_source_size (MODULE_T * module)
{
  FILE_T f = module->files.source.fd;
/* This is why WIN32 must open as "read binary". */
  return (lseek (f, 0, SEEK_END));
}

/*!
\brief append environment source lines
\param module module that reads source
\param str line to append
\param ref_l source line after which to append
\param line_num number of source line 'ref_l'
\param name either "prelude" or "postlude"
**/

static void append_environ (MODULE_T * module, char *str, SOURCE_LINE_T ** ref_l, int *line_num, char *name)
{
  char *text = new_string (str);
  while (text != NULL && text[0] != NULL_CHAR) {
    char *car = text;
    char *cdr = a68g_strchr (text, '!');
    int zero_line_num = 0;
    cdr[0] = NULL_CHAR;
    text = &cdr[1];
    (*line_num)++;
    snprintf (edit_line, BUFFER_SIZE, "%s\n", car);
    append_source_line (module, edit_line, ref_l, &zero_line_num, name);
  }
}

/*!
\brief read source file and make internal copy
\param module module that reads source
\return whether reading is satisfactory 
**/

static BOOL_T read_source_file (MODULE_T * module)
{
  SOURCE_LINE_T *ref_l = NULL;
  int line_num = 0, k, bytes_read;
  ssize_t l;
  FILE_T f = module->files.source.fd;
  char *prelude_start, *postlude, *buffer;
/* Prelude. */
  if (module->options.stropping == UPPER_STROPPING) {
    prelude_start = bold_prelude_start;
    postlude = bold_postlude;
  } else if (module->options.stropping == QUOTE_STROPPING) {
    prelude_start = quote_prelude_start;
    postlude = quote_postlude;
  } else {
    prelude_start = NULL;
    postlude = NULL;
  }
  append_environ (module, prelude_start, &ref_l, &line_num, "prelude");
/* Read the file into a single buffer, so we save on system calls. */
  line_num = 1;
  buffer = (char *) get_temp_heap_space ((unsigned) (8 + source_file_size));
  RESET_ERRNO;
  lseek (f, 0, SEEK_SET);
  ABNORMAL_END (errno != 0, "error while reading source file", NULL);
  RESET_ERRNO;
  bytes_read = io_read (f, buffer, source_file_size);
  ABNORMAL_END (errno != 0 || bytes_read != source_file_size, "error while reading source file", NULL);
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
    append_source_line (module, scan_buf, &ref_l, &line_num, module->files.source.name);
  }
/* Postlude. */
  append_environ (module, postlude, &ref_l, &line_num, "postlude");
/* Concatenate lines. */
  concatenate_lines (module->top_line);
/* Include files. */
  include_files (module->top_line);
  return (A68_TRUE);
}

/*!
\brief next_char get next character from internal copy of source file
\param module module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param allow_typo whether typographical display features are allowed
\return next char on input
**/

static char next_char (MODULE_T * module, SOURCE_LINE_T ** ref_l, char **ref_s, BOOL_T allow_typo)
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
    (*ref_l)->list = (module->options.nodemask & SOURCE_MASK ? A68_TRUE : A68_FALSE);
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
      ch = next_char (module, ref_l, ref_s, allow_typo);
    }
    return (ch);
  }
}

/*!
\brief find first character that can start a valid symbol
\param module module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
**/

static void get_good_char (MODULE_T * module, char *ref_c, SOURCE_LINE_T ** ref_l, char **ref_s)
{
  while (*ref_c != STOP_CHAR && (IS_SPACE (*ref_c) || (*ref_c == NULL_CHAR))) {
    if (*ref_l != NULL) {
      (*ref_l)->list = (module->options.nodemask & SOURCE_MASK ? A68_TRUE : A68_FALSE);
    }
    *ref_c = next_char (module, ref_l, ref_s, A68_FALSE);
  }
}

/*!
\brief handle a pragment (pragmat or comment)
\param module module that reads source
\param type type of pragment (#, CO, COMMENT, PR, PRAGMAT)
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
**/

static void pragment (MODULE_T * module, int type, SOURCE_LINE_T ** ref_l, char **ref_c)
{
#define INIT_BUFFER {chars_in_buf = 0; scan_buf[chars_in_buf] = NULL_CHAR;}
#define ADD_ONE_CHAR(ch) {scan_buf[chars_in_buf ++] = ch; scan_buf[chars_in_buf] = NULL_CHAR;}
  char c = **ref_c, *term_s = NULL, *start_c = *ref_c;
  SOURCE_LINE_T *start_l = *ref_l;
  int term_s_length, chars_in_buf;
  BOOL_T stop;
/* Set terminator. */
  if (module->options.stropping == UPPER_STROPPING) {
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
  } else if (module->options.stropping == QUOTE_STROPPING) {
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
  term_s_length = strlen (term_s);
/* Scan for terminator, and process pragmat items. */
  INIT_BUFFER;
  get_good_char (module, &c, ref_l, ref_c);
  stop = A68_FALSE;
  while (stop == A68_FALSE) {
    SCAN_ERROR (c == STOP_CHAR, start_l, start_c, ERROR_UNTERMINATED_PRAGMENT);
/* A ".." or '..' delimited string in a PRAGMAT. */
    if ((c == QUOTE_CHAR || (c == '\'' && module->options.stropping == UPPER_STROPPING))
        && (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL)) {
      char delim = c;
      BOOL_T eos = A68_FALSE;
      ADD_ONE_CHAR (c);
      c = next_char (module, ref_l, ref_c, A68_FALSE);
      while (!eos) {
        SCAN_ERROR (EOL (c), start_l, start_c, ERROR_LONG_STRING);
        if (c == delim) {
          ADD_ONE_CHAR (delim);
          c = next_char (module, ref_l, ref_c, A68_FALSE);
          save_state (module, *ref_l, *ref_c, c);
          if (c == delim) {
            c = next_char (module, ref_l, ref_c, A68_FALSE);
          } else {
            restore_state (module, ref_l, ref_c, &c);
            eos = A68_TRUE;
          }
        } else if (IS_PRINT (c)) {
          ADD_ONE_CHAR (c);
          c = next_char (module, ref_l, ref_c, A68_FALSE);
        } else {
          unworthy (start_l, start_c, c);
        }
      }
    }
/* On newline we empty the buffer and scan options when appropriate. */
    else if (EOL (c)) {
      if (type == STYLE_I_PRAGMAT_SYMBOL || type == BOLD_PRAGMAT_SYMBOL) {
        isolate_options (module, scan_buf, start_l);
      }
      INIT_BUFFER;
    } else if (IS_PRINT (c)) {
      ADD_ONE_CHAR (c);
    }
    if (chars_in_buf >= term_s_length) {
/* Check whether we encountered the terminator. */
      stop = (strcmp (term_s, &(scan_buf[chars_in_buf - term_s_length])) == 0);
    }
    c = next_char (module, ref_l, ref_c, A68_FALSE);
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
  case 'a':
    {
      return (FORMAT_ITEM_A);
    }
  case 'b':
    {
      return (FORMAT_ITEM_B);
    }
  case 'c':
    {
      return (FORMAT_ITEM_C);
    }
  case 'd':
    {
      return (FORMAT_ITEM_D);
    }
  case 'e':
    {
      return (FORMAT_ITEM_E);
    }
  case 'f':
    {
      return (FORMAT_ITEM_F);
    }
  case 'g':
    {
      return (FORMAT_ITEM_G);
    }
  case 'h':
    {
      return (FORMAT_ITEM_H);
    }
  case 'i':
    {
      return (FORMAT_ITEM_I);
    }
  case 'j':
    {
      return (FORMAT_ITEM_J);
    }
  case 'k':
    {
      return (FORMAT_ITEM_K);
    }
  case 'l':
  case '/':
    {
      return (FORMAT_ITEM_L);
    }
  case 'm':
    {
      return (FORMAT_ITEM_M);
    }
  case 'n':
    {
      return (FORMAT_ITEM_N);
    }
  case 'o':
    {
      return (FORMAT_ITEM_O);
    }
  case 'p':
    {
      return (FORMAT_ITEM_P);
    }
  case 'q':
    {
      return (FORMAT_ITEM_Q);
    }
  case 'r':
    {
      return (FORMAT_ITEM_R);
    }
  case 's':
    {
      return (FORMAT_ITEM_S);
    }
  case 't':
    {
      return (FORMAT_ITEM_T);
    }
  case 'u':
    {
      return (FORMAT_ITEM_U);
    }
  case 'v':
    {
      return (FORMAT_ITEM_V);
    }
  case 'w':
    {
      return (FORMAT_ITEM_W);
    }
  case 'x':
    {
      return (FORMAT_ITEM_X);
    }
  case 'y':
    {
      return (FORMAT_ITEM_Y);
    }
  case 'z':
    {
      return (FORMAT_ITEM_Z);
    }
  case '+':
    {
      return (FORMAT_ITEM_PLUS);
    }
  case '-':
    {
      return (FORMAT_ITEM_MINUS);
    }
  case POINT_CHAR:
    {
      return (FORMAT_ITEM_POINT);
    }
  case '%':
    {
      return (FORMAT_ITEM_ESCAPE);
    }
  default:
    {
      return (0);
    }
  }
}

/* Macros. */

#define SCAN_DIGITS(c)\
  while (IS_DIGIT (c)) {\
    (sym++)[0] = (c);\
    (c) = next_char (module, ref_l, ref_s, A68_TRUE);\
  }

#define SCAN_EXPONENT_PART(c)\
  (sym++)[0] = EXPONENT_CHAR;\
  (c) = next_char (module, ref_l, ref_s, A68_TRUE);\
  if ((c) == '+' || (c) == '-') {\
    (sym++)[0] = (c);\
    (c) = next_char (module, ref_l, ref_s, A68_TRUE);\
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

static BOOL_T whether_exp_char (MODULE_T * m, SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T res = A68_FALSE;
  char exp_syms[3];
  if (m->options.stropping == UPPER_STROPPING) {
    exp_syms[0] = EXPONENT_CHAR;
    exp_syms[1] = TO_UPPER (EXPONENT_CHAR);
    exp_syms[2] = NULL_CHAR;
  } else {
    exp_syms[0] = TO_UPPER (EXPONENT_CHAR);
    exp_syms[1] = ESCAPE_CHAR;
    exp_syms[2] = NULL_CHAR;
  }
  save_state (m, *ref_l, *ref_s, *ch);
  if (strchr (exp_syms, *ch) != NULL) {
    *ch = next_char (m, ref_l, ref_s, A68_TRUE);
    res = (strchr ("+-0123456789", *ch) != NULL);
  }
  restore_state (m, ref_l, ref_s, ch);
  return (res);
}

/*!
\brief whether input shows radix character
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\return same
**/

static BOOL_T whether_radix_char (MODULE_T * m, SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T res = A68_FALSE;
  save_state (m, *ref_l, *ref_s, *ch);
  if (m->options.stropping == QUOTE_STROPPING) {
    if (*ch == TO_UPPER (RADIX_CHAR)) {
      *ch = next_char (m, ref_l, ref_s, A68_TRUE);
      res = (strchr ("0123456789ABCDEF", *ch) != NULL);
    }
  } else {
    if (*ch == RADIX_CHAR) {
      *ch = next_char (m, ref_l, ref_s, A68_TRUE);
      res = (strchr ("0123456789abcdef", *ch) != NULL);
    }
  }
  restore_state (m, ref_l, ref_s, ch);
  return (res);
}

/*!
\brief whether input shows decimal point
\param m module that reads source
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\return same
**/

static BOOL_T whether_decimal_point (MODULE_T * m, SOURCE_LINE_T ** ref_l, char **ref_s, char *ch)
{
  BOOL_T res = A68_FALSE;
  save_state (m, *ref_l, *ref_s, *ch);
  if (*ch == POINT_CHAR) {
    char exp_syms[3];
    if (m->options.stropping == UPPER_STROPPING) {
      exp_syms[0] = EXPONENT_CHAR;
      exp_syms[1] = TO_UPPER (EXPONENT_CHAR);
      exp_syms[2] = NULL_CHAR;
    } else {
      exp_syms[0] = TO_UPPER (EXPONENT_CHAR);
      exp_syms[1] = ESCAPE_CHAR;
      exp_syms[2] = NULL_CHAR;
    }
    *ch = next_char (m, ref_l, ref_s, A68_TRUE);
    if (strchr (exp_syms, *ch) != NULL) {
      *ch = next_char (m, ref_l, ref_s, A68_TRUE);
      res = (strchr ("+-0123456789", *ch) != NULL);
    } else {
      res = (strchr ("0123456789", *ch) != NULL);
    }
  }
  restore_state (m, ref_l, ref_s, ch);
  return (res);
}

/*!
\brief get next token from internal copy of source file.
\param module module that reads source
\param in_format are we scanning a format text
\param ref_l source line we're scanning
\param ref_s character (in source line) we're scanning
\param start_l line where token starts
\param start_c character where token starts
\param att attribute designated to token
**/

static void get_next_token (MODULE_T * module, BOOL_T in_format, SOURCE_LINE_T ** ref_l, char **ref_s, SOURCE_LINE_T ** start_l, char **start_c, int *att)
{
  char c = **ref_s, *sym = scan_buf;
  sym[0] = NULL_CHAR;
  get_good_char (module, &c, ref_l, ref_s);
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
    if (module->options.stropping == UPPER_STROPPING) {
      format_items = "/%\\+-.abcdefghijklmnopqrstuvwxyz";
    } else {                    /* if (module->options.stropping == QUOTE_STROPPING) */
      format_items = "/%\\+-.ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    }
    if (a68g_strchr (format_items, c) != NULL) {
/* General format items. */
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      *att = get_format_item (c);
      next_char (module, ref_l, ref_s, A68_FALSE);
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
    if (module->options.stropping == UPPER_STROPPING) {
/* Upper case word - bold tag. */
      while (IS_UPPER (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (module, ref_l, ref_s, A68_FALSE);
      }
      sym[0] = NULL_CHAR;
      *att = BOLD_TAG;
    } else if (module->options.stropping == QUOTE_STROPPING) {
      while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
        (sym++)[0] = c;
        c = next_char (module, ref_l, ref_s, A68_TRUE);
      }
      sym[0] = NULL_CHAR;
      *att = IDENTIFIER;
    }
  } else if (c == '\'') {
/* Quote, uppercase word, quote - bold tag. */
    int k = 0;
    c = next_char (module, ref_l, ref_s, A68_FALSE);
    while (IS_UPPER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      k++;
      c = next_char (module, ref_l, ref_s, A68_TRUE);
    }
    SCAN_ERROR (k == 0, *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    sym[0] = NULL_CHAR;
    *att = BOLD_TAG;
/* Skip terminating quote, or complain if it is not there. */
    SCAN_ERROR (c != '\'', *start_l, *start_c, ERROR_QUOTED_BOLD_TAG);
    c = next_char (module, ref_l, ref_s, A68_FALSE);
  } else if (IS_LOWER (c)) {
/* Lower case word - identifier. */
    while (IS_LOWER (c) || IS_DIGIT (c) || c == '_') {
      (sym++)[0] = c;
      c = next_char (module, ref_l, ref_s, A68_TRUE);
    }
    sym[0] = NULL_CHAR;
    *att = IDENTIFIER;
  } else if (c == POINT_CHAR) {
/* Begins with a point symbol - point, dotdot, L REAL denotation. */
    if (whether_decimal_point (module, ref_l, ref_s, &c)) {
      (sym++)[0] = '0';
      (sym++)[0] = POINT_CHAR;
      c = next_char (module, ref_l, ref_s, A68_TRUE);
      SCAN_DIGITS (c);
      if (whether_exp_char (module, ref_l, ref_s, &c)) {
        SCAN_EXPONENT_PART (c);
      }
      sym[0] = NULL_CHAR;
      *att = REAL_DENOTATION;
    } else {
      c = next_char (module, ref_l, ref_s, A68_TRUE);
      if (c == POINT_CHAR) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = DOTDOT_SYMBOL;
        c = next_char (module, ref_l, ref_s, A68_FALSE);
      } else {
        (sym++)[0] = POINT_CHAR;
        sym[0] = NULL_CHAR;
        *att = POINT_SYMBOL;
      }
    }
  } else if (IS_DIGIT (c)) {
/* Something that begins with a digit - L INT denotation, L REAL denotation. */
    SCAN_DIGITS (c);
    if (whether_decimal_point (module, ref_l, ref_s, &c)) {
      c = next_char (module, ref_l, ref_s, A68_TRUE);
      if (whether_exp_char (module, ref_l, ref_s, &c)) {
        (sym++)[0] = POINT_CHAR;
        (sym++)[0] = '0';
        SCAN_EXPONENT_PART (c);
        *att = REAL_DENOTATION;
      } else {
        (sym++)[0] = POINT_CHAR;
        SCAN_DIGITS (c);
        if (whether_exp_char (module, ref_l, ref_s, &c)) {
          SCAN_EXPONENT_PART (c);
        }
        *att = REAL_DENOTATION;
      }
    } else if (whether_exp_char (module, ref_l, ref_s, &c)) {
      SCAN_EXPONENT_PART (c);
      *att = REAL_DENOTATION;
    } else if (whether_radix_char (module, ref_l, ref_s, &c)) {
      (sym++)[0] = c;
      c = next_char (module, ref_l, ref_s, A68_TRUE);
      if (module->options.stropping == UPPER_STROPPING) {
        while (IS_DIGIT (c) || strchr ("abcdef", c) != NULL) {
          (sym++)[0] = c;
          c = next_char (module, ref_l, ref_s, A68_TRUE);
        }
      } else {
        while (IS_DIGIT (c) || strchr ("ABCDEF", c) != NULL) {
          (sym++)[0] = c;
          c = next_char (module, ref_l, ref_s, A68_TRUE);
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
      c = next_char (module, ref_l, ref_s, A68_FALSE);
      while (c != QUOTE_CHAR && c != STOP_CHAR) {
        SCAN_ERROR (EOL (c), *start_l, *start_c, ERROR_LONG_STRING);
        (sym++)[0] = c;
        c = next_char (module, ref_l, ref_s, A68_FALSE);
      }
      SCAN_ERROR (*ref_l == NULL, *start_l, *start_c, ERROR_UNTERMINATED_STRING);
      c = next_char (module, ref_l, ref_s, A68_FALSE);
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
    next_char (module, ref_l, ref_s, A68_FALSE);
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '|') {
/* Bar. */
    (sym++)[0] = c;
    c = next_char (module, ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      next_char (module, ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '!' && module->options.stropping == QUOTE_STROPPING) {
/* Bar, will be replaced with modern variant.
   For this reason ! is not a MONAD with quote-stropping. */
    (sym++)[0] = '|';
    c = next_char (module, ref_l, ref_s, A68_FALSE);
    if (c == ':') {
      (sym++)[0] = c;
      next_char (module, ref_l, ref_s, A68_FALSE);
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == ':') {
/* Colon, semicolon, IS, ISNT. */
    (sym++)[0] = c;
    c = next_char (module, ref_l, ref_s, A68_FALSE);
    if (c == '=') {
      (sym++)[0] = c;
      if ((c = next_char (module, ref_l, ref_s, A68_FALSE)) == ':') {
        (sym++)[0] = c;
        c = next_char (module, ref_l, ref_s, A68_FALSE);
      }
    } else if (c == '/') {
      (sym++)[0] = c;
      if ((c = next_char (module, ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
        if ((c = next_char (module, ref_l, ref_s, A68_FALSE)) == ':') {
          (sym++)[0] = c;
          c = next_char (module, ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      if ((c = next_char (module, ref_l, ref_s, A68_FALSE)) == '=') {
        (sym++)[0] = c;
      }
    }
    sym[0] = NULL_CHAR;
    *att = 0;
  } else if (c == '=') {
/* Operator starting with "=". */
    char *scanned = sym;
    (sym++)[0] = c;
    c = next_char (module, ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NULL) {
      (sym++)[0] = c;
      c = next_char (module, ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (module, ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (module, ref_l, ref_s, A68_FALSE);
        if (strlen (sym) < 4 && c == '=') {
          (sym++)[0] = '=';
          next_char (module, ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (module, ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        next_char (module, ref_l, ref_s, A68_FALSE);
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
    c = next_char (module, ref_l, ref_s, A68_FALSE);
    if (a68g_strchr (NOMADS, c) != NULL) {
      (sym++)[0] = c;
      c = next_char (module, ref_l, ref_s, A68_FALSE);
    }
    if (c == '=') {
      (sym++)[0] = c;
      if (next_char (module, ref_l, ref_s, A68_FALSE) == ':') {
        (sym++)[0] = ':';
        c = next_char (module, ref_l, ref_s, A68_FALSE);
        if (strlen (scanned) < 4 && c == '=') {
          (sym++)[0] = '=';
          next_char (module, ref_l, ref_s, A68_FALSE);
        }
      }
    } else if (c == ':') {
      (sym++)[0] = c;
      sym[0] = NULL_CHAR;
      if (next_char (module, ref_l, ref_s, A68_FALSE) == '=') {
        (sym++)[0] = '=';
        sym[0] = NULL_CHAR;
        next_char (module, ref_l, ref_s, A68_FALSE);
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
    p[0] = TO_LOWER (p[0]);
  }
}

/*!
\brief construct a linear list of tokens
\param module module that reads source
\param root node where to insert new symbol
\param level current recursive descent depth
\param in_format whether we scan a format
\param l current source line
\param s current character in source line
\param start_l source line where symbol starts
\param start_c character where symbol starts
**/

static void tokenise_source (MODULE_T * module, NODE_T ** root, int level, BOOL_T in_format, SOURCE_LINE_T ** l, char **s, SOURCE_LINE_T ** start_l, char **start_c)
{
  while (l != NULL && !stop_scanner) {
    int att = 0;
    get_next_token (module, in_format, l, s, start_l, start_c, &att);
    if (scan_buf[0] == STOP_CHAR) {
      stop_scanner = A68_TRUE;
    } else if (strlen (scan_buf) > 0 || att == ROW_CHAR_DENOTATION || att == LITERAL) {
      KEYWORD_T *kw = find_keyword (top_keyword, scan_buf);
      char *c = NULL;
      BOOL_T make_node = A68_TRUE;
      if (!(kw != NULL && att != ROW_CHAR_DENOTATION)) {
        if (att == IDENTIFIER) {
          make_lower_case (scan_buf);
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
            pragment (module, ATTRIBUTE (kw), l, s);
            make_node = A68_FALSE;
          } else if (att == STYLE_I_PRAGMAT_SYMBOL || att == BOLD_PRAGMAT_SYMBOL) {
            pragment (module, ATTRIBUTE (kw), l, s);
            if (!stop_scanner) {
              isolate_options (module, scan_buf, *start_l);
              set_options (module, module->options.list, A68_FALSE);
              make_node = A68_FALSE;
            }
          }
        }
      }
/* Add token to the tree. */
      if (make_node) {
        NODE_T *q = new_node ();
        MASK (q) = module->options.nodemask;
        LINE (q) = *start_l;
        INFO (q)->char_in_line = *start_c;
        PRIO (INFO (q)) = 0;
        INFO (q)->PROCEDURE_LEVEL = 0;
        ATTRIBUTE (q) = att;
        SYMBOL (q) = c;
        if (module->options.reductions) {
          WRITELN (STDOUT_FILENO, "\"");
          WRITE (STDOUT_FILENO, c);
          WRITE (STDOUT_FILENO, "\"");
        }
        PREVIOUS (q) = *root;
        SUB (q) = NEXT (q) = NULL;
        SYMBOL_TABLE (q) = NULL;
        MODULE (INFO (q)) = module;
        MOID (q) = NULL;
        TAX (q) = NULL;
        if (*root != NULL) {
          NEXT (*root) = q;
        }
        if (module->top_node == NULL) {
          module->top_node = q;
        }
        *root = q;
      }
/* 
Redirection in tokenising formats. The scanner is a recursive-descent type as
to know when it scans a format text and when not. 
*/
      if (in_format && att == FORMAT_DELIMITER_SYMBOL) {
        return;
      } else if (!in_format && att == FORMAT_DELIMITER_SYMBOL) {
        tokenise_source (module, root, level + 1, A68_TRUE, l, s, start_l, start_c);
      } else if (in_format && open_embedded_clause (att)) {
        NODE_T *z = PREVIOUS (*root);
        if (z != NULL && (WHETHER (z, FORMAT_ITEM_N) || WHETHER (z, FORMAT_ITEM_G)
                          || WHETHER (z, FORMAT_ITEM_H)
                          || WHETHER (z, FORMAT_ITEM_F))) {
          tokenise_source (module, root, level, A68_FALSE, l, s, start_l, start_c);
        } else if (att == OPEN_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (module->options.brackets && att == SUB_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        } else if (module->options.brackets && att == ACCO_SYMBOL) {
          ATTRIBUTE (*root) = FORMAT_OPEN_SYMBOL;
        }
      } else if (!in_format && level > 0 && open_embedded_clause (att)) {
        tokenise_source (module, root, level + 1, A68_FALSE, l, s, start_l, start_c);
      } else if (!in_format && level > 0 && close_embedded_clause (att)) {
        return;
      } else if (in_format && att == CLOSE_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (module->options.brackets && in_format && att == BUS_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      } else if (module->options.brackets && in_format && att == OCCA_SYMBOL) {
        ATTRIBUTE (*root) = FORMAT_CLOSE_SYMBOL;
      }
    }
  }
}

/*!
\brief tokenise source file, build initial syntax tree
\param module module that reads source
\return whether tokenising ended satisfactorily
**/

BOOL_T lexical_analyzer (MODULE_T * module)
{
  SOURCE_LINE_T *l, *start_l = NULL;
  char *s = NULL, *start_c = NULL;
  NODE_T *root = NULL;
  scan_buf = NULL;
  max_scan_buf_length = source_file_size = get_source_size (module);
/* Errors in file? */
  if (max_scan_buf_length == 0) {
    return (A68_FALSE);
  }
  max_scan_buf_length += strlen (bold_prelude_start) + strlen (bold_postlude);
  max_scan_buf_length += strlen (quote_prelude_start) + strlen (quote_postlude);
/* Allocate a scan buffer with 8 bytes extra space. */
  scan_buf = (char *) get_temp_heap_space ((unsigned) (8 + max_scan_buf_length));
/* Errors in file? */
  if (!read_source_file (module)) {
    return (A68_FALSE);
  }
/* Start tokenising. */
  read_error = A68_FALSE;
  stop_scanner = A68_FALSE;
  if ((l = module->top_line) != NULL) {
    s = l->string;
  }
  tokenise_source (module, &root, 0, A68_FALSE, &l, &s, &start_l, &start_c);
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

void get_refinements (MODULE_T * z)
{
  NODE_T *p = z->top_node;
  z->top_refinement = NULL;
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
    REFINEMENT_T *new_one = (REFINEMENT_T *) get_fixed_heap_space (ALIGNED_SIZE_OF (REFINEMENT_T)), *x;
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
    x = z->top_refinement;
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
      NEXT (new_one) = z->top_refinement;
      z->top_refinement = new_one;
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

void put_refinements (MODULE_T * z)
{
  REFINEMENT_T *x;
  NODE_T *p, *point;
/* If there are no refinements, there's little to do. */
  if (z->top_refinement == NULL) {
    return;
  }
/* Initialisation. */
  x = z->top_refinement;
  while (x != NULL) {
    x->applications = 0;
    FORWARD (x);
  }
/* Before we introduce infinite loops, find where closing-prelude starts. */
  p = z->top_node;
  while (p != NULL && IN_PRELUDE (p)) {
    FORWARD (p);
  }
  while (p != NULL && !IN_PRELUDE (p)) {
    FORWARD (p);
  }
  ABNORMAL_END (p == NULL, ERROR_INTERNAL_CONSISTENCY, NULL);
  point = p;
/* We need to substitute until the first point. */
  p = z->top_node;
  while (p != NULL && ATTRIBUTE (p) != POINT_SYMBOL) {
    if (WHETHER (p, IDENTIFIER)) {
/* See if we can find its definition. */
      REFINEMENT_T *y = NULL;
      x = z->top_refinement;
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
  if (a68_prog.error_count == 0) {
    x = z->top_refinement;
    while (x != NULL) {
      if (x->applications == 0) {
        diagnostic_node (A68_SYNTAX_ERROR, x->node_defined, ERROR_REFINEMENT_NOT_APPLIED, NULL);
      }
      FORWARD (x);
    }
  }
}
