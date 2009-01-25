/*!
\file help.c
\brief interactive help
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

#include "algol68g.h"

typedef struct A68_INFO A68_INFO;

struct A68_INFO
{
  char *cat;
  char *term;
  char *def;
};

static A68_INFO info_text[] = {
  {"monitor", "breakpoint clear [all]", "clear breakpoints and watchpoint expression"},
  {"monitor", "breakpoint clear breakpoints", "clear breakpoints"},
  {"monitor", "breakpoint clear watchpoint", "clear watchpoint expression"},
  {"monitor", "breakpoint [list]", "list breakpoints"},
  {"monitor", "breakpoint \"n\" clear", "clear breakpoints in line \"n\""},
  {"monitor", "breakpoint \"n\" if \"expression\"", "break in line \"n\" when expression evaluates to true"},
  {"monitor", "breakpoint \"n\"", "set breakpoints in line \"n\""},
  {"monitor", "breakpoint watch \"expression\"", "break on watchpoint expression when it evaluates to true"},
  {"monitor", "calls [n]", "print \"n\" frames in the call stack (default n=3)"},
  {"monitor", "continue, resume", "continue execution"},
  {"monitor", "do \"command\", exec \"command\"", "pass \"command\" to the shell and print return code"},
  {"monitor", "elems [n]", "print first \"n\" elements of rows (default n=24)"},
  {"monitor", "evaluate \"expression\", x \"expression\"", "print result of \"expression\""},
  {"monitor", "examine \"n\"", "print value of symbols named \"n\" in the call stack"},
  {"monitor", "exit, hx, quit", "terminates the program"},
  {"monitor", "finish, out", "continue execution until current procedure incarnation is finished"},
  {"monitor", "frame 0", "set current stack frame to top of frame stack"},
  {"monitor", "frame \"n\"", "set current stack frame to \"n\""},
  {"monitor", "frame", "print contents of the current stack frame"},
  {"monitor", "heap \"n\"", "print contents of the heap with address not greater than \"n\""},
  {"monitor", "help [expression]", "print brief help text"},
  {"monitor", "ht", "halts typing to standard output"},
  {"monitor", "list [n]", "show \"n\" lines around the interrupted line (default n=10)"},
  {"monitor", "next", "continue execution to next interruptable unit (do not enter routine-texts)"},
  {"monitor", "prompt \"s\"", "set prompt to \"s\""},
  {"monitor", "rerun, restart", "restarts a program without resetting breakpoints"},
  {"monitor", "reset", "restarts a program and resets breakpoints"},
  {"monitor", "rt", "resumes typing to standard output"},
  {"monitor", "sizes", "print size of memory segments"},
  {"monitor", "stack [n]", "print \"n\" frames in the stack (default n=3)"},
  {"monitor", "step", "continue execution to next interruptable unit"},
  {"monitor", "until \"n\"", "continue execution until line number \"n\" is reached"},
  {"monitor", "where", "print the interrupted line"},
  {"monitor", "xref \"n\"", "give detailed information on source line \"n\""},
  {"options", "--assertions, --noassertions", "switch elaboration of assertions on or off"},
  {"options", "--backtrace, --nobacktrace", "switch stack backtracing in case of a runtime error"},
  {"options", "--boldstropping", "set stropping mode to bold stropping"},
  {"options", "--brackets", "consider [ .. ] and { .. } as equivalent to ( .. )"},
  {"options", "--check, --norun", "check syntax only, interpreter does not start"},
  {"options", "--debug, --monitor", "start execution in the debugger and debug in case of runtime error"},
  {"options", "--echo string", "echo \"string\" to standard output"},
  {"options", "--execute unit", "execute algol 68 unit \"unit\""},
  {"options", "--exit, --", "ignore next options"},
  {"options", "--extensive", "make extensive listing"},
  {"options", "--file string", "accept string as generic filename"},
  {"options", "--frame \"number\"", "set frame stack size to \"number\""},
  {"options", "--handles \"number\"", "set handle space size to \"number\""},
  {"options", "--heap \"number\"", "set heap size to \"number\""},
  {"options", "--listing", "make concise listing"},
  {"options", "--moids", "make overview of moids in listing file"},
  {"options", "--pedantic", "equivalent to --warnings --portcheck"},
  {"options", "--portcheck, --noportcheck", "switch portability warnings on or off"},
  {"options", "--pragmats, --nopragmats", "switch elaboration of pragmat items on or off"},
  {"options", "--precision \"number\"", "set precision for long long modes to \"number\" significant digits"},
  {"options", "--preludelisting", "make a listing of preludes"},
  {"options", "--print unit", "print value yielded by algol 68 unit \"unit\""},
  {"options", "--quotestropping", "set stropping mode to quote stropping"},
  {"options", "--reductions", "print parser reductions"},
  {"options", "--run", "override --check/--norun options"},
  {"options", "--source, --nosource", "switch listing of source lines in listing file on or off"},
  {"options", "--stack \"number\"", "set expression stack size to \"number\""},
  {"options", "--statistics", "print statistics in listing file"},
  {"options", "--timelimit \"number\"", "interrupt the interpreter after \"number\" seconds"},
  {"options", "--trace, --notrace", "switch tracing of a running program on or off"},
  {"options", "--tree, --notree", "switch syntax tree listing in listing file on or off"},
  {"options", "--unused", "make an overview of unused tags in the listing file"},
  {"options", "--verbose", "inform on program actions"},
  {"options", "--version", "state version of the running copy"},
  {"options", "--warnings, --nowarnings", "switch warning diagnostics on or off"},
  {"options", "--xref, --noxref", "switch cross reference in the listing file on or off"},
  {"syntax", "actual-bounds", "[meek-integer-unit `:'] meek-integer-unit. # default lower bound is 1 #"},
  {"syntax", "actual-parameter", "[strong-unit]"},
  {"syntax", "assertion", "`ASSERT' meek-boolean-enclosed-clause"},
  {"syntax", "assignation", "soft-tertiary `:=' strong-unit"},
  {"syntax", "bits-denotation", "[`LONG'-sequence, `SHORT'-sequence] digit-sequence `r' bits-digit-sequence"},
  {"syntax", "bits-pattern", "replicator `r' integer-mould"},
  {"syntax", "boolean-denotation", "`TRUE', `FALSE'"},
  {"syntax", "boolean-pattern", "`b' `(' row-character-denotation `,' row-character-denotation `)'"},
  {"syntax", "call", "meek-primary `(' actual-parameter-list')'"},
  {"syntax", "cast", "(formal-declarer, `VOID') strong-enclosed-clause"},
  {"syntax", "character-denotation", "`\"' string-item `\"'"},
  {"syntax", "choice-pattern", "`c' `(' row-character-denotation-list `)'"},
  {"syntax", "closed-clause", "`BEGIN' serial-clause `END'"},
  {"syntax", "collateral-clause", "`BEGIN' [unit-list] `END'"},
  {"syntax", "collection", "`(' picture-list `)'"},
  {"syntax", "comment", "comment-symbol, character-sequence, comment-symbol"},
  {"syntax", "comment-symbol", "`#', `CO', `COMMENT'"},
  {"syntax", "complex-pattern", "real-pattern [`s']`i' [insertion] real-pattern"},
  {"syntax", "conditional-clause", "`IF' meek-boolean-enquiry-clause `THEN' serial-clause [(`ELIF') meek-boolean-enquiry-clause `THEN' serial-clause)-sequence] [`ELSE' serial-clause] `FI'"},
  {"syntax", "conditional-function", "meek-boolean-tertiary ((`ANDF', `ANDTH', `THEF'), (`ORF', `OREL', `ELSF')) meek-boolean-tertiary"},
  {"syntax", "declaration", "mode-declaration, identity-declaration, variable-declaration, procedure-declaration, procedure-variable-declaration, operator-declaration, priority-declaration"},
  {"syntax", "declarer-identifier", "uppercase-letter-sequence"},
  {"syntax", "denotation", "integer-denotation, real-denotation, boolean-denotation, bits-denotation, character-denotation, row-of-character-denotation, VOID-denotation"},
  {"syntax", "dyadic operator", "uppercase-letter-sequence, (monad, nomad)[nomad][`:=', `=:']"},
  {"syntax", "enclosed-clause", "closed-clause, collateral-clause, parallel-clause, conditional-clause, integer-case-clause, united-case-clause, loop-clause"},
  {"syntax", "enquiry-clause", "[initialiser-series `;'] [(unit `;')-sequence] unit"},
  {"syntax", "factor", "[monadic-operator-sequence] secondary, formula"},
  {"syntax", "formal-bounds</b>, <b>virtual-bounds", "[`:']"},
  {"syntax", "format-pattern", "`f' meek-format-enclosed-clause"},
  {"syntax", "format-text", "`$' picture-list `$'"},
  {"syntax", "formula", "monadic-operator-sequence firm-secondary, firm-factor dyadic-operator firm-factor"},
  {"syntax", "general-pattern", "`g'[strong-row-of-integer-enclosed-clause], `h'[strong-row-of-integer-enclosed-clause]"},
  {"syntax", "generator", "(`HEAP', `LOC') actual-declarer"},
  {"syntax", "identifier", "lowercase-letter [(lowercase letter, digit)-sequence]"},
  {"syntax", "identity-declaration", "formal-declarer (identifier `=' strong-unit)-list"},
  {"syntax", "identity-relation", "soft-tertiary (`IS', `:=:'), (`ISNT', `:/=:') soft-tertiary"},
  {"syntax", "indexer", "[meek-integer-unit] [`:'] [meek-integer-unit] [revised-lower-bound]"},
  {"syntax", "initialiser-series", "(unit, declaration-list) [`;' initialiser-series]. "},
  {"syntax", "insertion", "(replicator `k', [replicator] `l', [replicator] `p', [replicator] `x', [replicator] `q', [replicator] row-of-character-denotation)-sequence"},
  {"syntax", "integer-case-clause", "`CASE' meek-integer-enquiry-clause `IN' unit-list [(`OUSE' meek-integer-enquiry-clause `IN' unit-list)-sequence] [`OUT' serial-clause] `ESAC'"},
  {"syntax", "integer-denotation", "[`LONG'-sequence, `SHORT'-sequence] digit-sequence"},
  {"syntax", "integer-mould", "[replicator] [`s'] (`z', `d') [([replicator] [`s'] (`z', `d'), insertion)-sequence]"},
  {"syntax", "integer-pattern", "[sign-mould] integer-mould"},
  {"syntax", "jump", "[`GOTO'] label"},
  {"syntax", "labelled-unit-series", "[([label `:'] unit (';', `EXIT'))-sequence] unit"},
  {"syntax", "loop-clause", "[`FOR' identifier] [`FROM' meek-integer-unit] [`BY' meek-integer-unit] [(`TO', `DOWNTO') meek-integer-unit] [`WHILE' meek-boolean-enquiry-clause] `DO' (serial-clause [`UNTIL' meek-boolean-enquiry-clause]), `UNTIL' meek-boolean-enquiry-clause `OD'"},
  {"syntax", "monad", "`+', `-', `!', `?', `%', `^', `&', `~'"},
  {"syntax", "monadic operator", "uppercase-letter-sequence, monad[nomad][`:=', `=:']"},
  {"syntax", "nihil", "`NIL'"},
  {"syntax", "nomad", "`<', `>', `/', `=', `*'"},
  {"syntax", "operator-declaration", "`OP' (operator `=' routine-text)-list, `OP' [`(' formal-declarer-list `)' (formal-declarer, `VOID')] (operator `=' strong-unit)-list"},
  {"syntax", "operator", "monadic operator, dyadic operator"},
  {"syntax", "parallel-clause", "`PAR' `BEGIN' unit-list `END'"},
  {"syntax", "particular-program", "[(label `:')-sequence] enclosed-clause"},
  {"syntax", "pattern", "general-pattern, integer-pattern, real-pattern, complex-pattern, bits-pattern, string-pattern, boolean-pattern, choice-pattern, format-pattern"},
  {"syntax", "picture", "insertion, pattern, collection, replicator collection"},
  {"syntax", "pragmat", "pragmat-symbol, pragmat-item-sequence, pragmat-symbol"},
  {"syntax", "pragmat-symbol", "`PR', `PRAGMAT'"},
  {"syntax", "pragment", "pragmat, comment"},
  {"syntax", "primary", "enclosed-clause, identifier, call, slice, cast, format-text, denotation"},
  {"syntax", "primitive-declarer", "`INT', `REAL', `COMPL', `COMPLEX', `BOOL', `CHAR', `STRING', `BITS', `BYTES', `FORMAT', `FILE', `PIPE', `CHANNEL', `SEMA', 'SOUND'"},
  {"syntax", "priority-declaration", "`PRIO' (operator `=' digit)-list"},
  {"syntax", "procedure-declaration", "`PROC' (identifier `=' routine-text)-list"},
  {"syntax", "procedure-variable-declaration", "[`HEAP', `LOC'] `PROC' (identifier `:=' routine-text)-list"},
  {"syntax", "real-denotation", "[`LONG'-sequence, `SHORT'-sequence] digit-sequence `.' digit-sequence[`E' [`+', `-'] digit-sequence]"},
  {"syntax", "real-pattern", "[sign-mould] [`s'] `.' [insertion] integer-mould [[`s']`e' [insertion] [sign-mould] integer-mould]"},
  {"syntax", "refined program", "paragraph `.', refinement definition sequence. "},
  {"syntax", "refinement definition", "identifier `:' paragraph `.'"},
  {"syntax", "replicator", "integer-denotation, `n' meek-integer-enclosed-clause"},
  {"syntax", "revised-lower-bound", "(`AT', `@') meek-integer-unit"},
  {"syntax", "routine-text", "[`(' (formal-declarer identifier)-list `)'] formal-declarer `:' strong-unit"},
  {"syntax", "row-of-character-denotation", "`\"' string-item `\"'"},
  {"syntax", "secondary", "primary, selection, generator"},
  {"syntax", "selection", "identifier `OF' secondary"},
  {"syntax", "serial-clause", "[initialiser-series `;'] labelled-unit-series"},
  {"syntax", "sign-mould", "[replicator] [`s'] (`z', `d') [([replicator] [`s'] (`z', `d'), insertion)-sequence] (`+', `-')"},
  {"syntax", "skip", "`SKIP'"},
  {"syntax", "slice", "weak-primary `[' indexer-list `]'"},
  {"syntax", "specified-unit", "`(' (formal-declarer, `VOID') [identifier] `)' `:' unit"},
  {"syntax", "stowed-function", "(`DIAG', `TRNSP', `ROW', `COL') weak-tertiary, meek-integral-tertiary (`DIAG', `ROW', `COL') weak-tertiary"},
  {"syntax", "string-item", "character, \"\""},
  {"syntax", "string-pattern", "[replicator] [`s'] `a' [([replicator] [`s'] `a', insertion)-sequence]"},
  {"syntax", "tertiary", "secondary, nihil, formula, stowed-function"},
  {"syntax", "united-case-clause", "`CASE' meek-united-enquiry-clause `IN' specified-unit-list [(`OUSE' meek-united-enquiry-clause `IN' specified-unit-list)-sequence] [`OUT' serial-clause] `ESAC'"},
  {"syntax", "unit", "tertiary, assignation, routine-text, identity-relation, jump, skip, assertion, conditional-function"},
  {"syntax", "variable-declaration", "[`HEAP', `LOC'] actual-declarer (identifier `:=' strong-unit)-list"},
  {"syntax", "victal-declarer", "(`LONG'-sequence, `SHORT'-sequence) primitive-declarer, declarer-identifier, `REF' virtual-declarer, `[' victal-bounds-list `]' victal-declarer, `FLEX' `[' victal-bounds-list `]' victal-declarer, `STRUCT' `(' (victal-declarer identifier)-list `)', `UNION' `(' (formal-declarer, `VOID')-list `)', `PROC' [`(' formal-declarer-list `)'] (formal-declarer, `VOID')"},
  {"syntax", "victal", "virtual, actual, formal"},
  {"syntax", "VOID-denotation", "`EMPTY'"},
  {NULL, NULL, NULL}
};

/*!
\brief print_info
\param f file number
\param prompt prompt text
\param k index of info item to print
**/

static void print_info (FILE_T f, char *prompt, int k)
{
  if (prompt != NULL) {
    snprintf (output_line, BUFFER_SIZE, "%s %s: %s.", prompt, info_text[k].term, info_text[k].def);
  } else {
    snprintf (output_line, BUFFER_SIZE, "%s: %s.", info_text[k].term, info_text[k].def);
  }
  WRITELN (f, output_line);
}

/*!
\brief apropos
\param f file number
\param prompt prompt text
\param info item to print, NULL prints all
**/

void apropos (FILE_T f, char *prompt, char *item)
{
  int k, n = 0;
  if (item == NULL) {
    for (k = 0; info_text[k].cat != NULL; k++) {
      print_info (f, prompt, k);
    }
    return;
  }
  for (k = 0; info_text[k].cat != NULL; k++) {
    if (grep_in_string (item, info_text[k].cat, NULL, NULL) == 0) {
      print_info (f, prompt, k);
      n++;
    }
  }
  if (n > 0) {
    return;
  }
  for (k = 0; info_text[k].cat != NULL; k++) {
    if (grep_in_string (item, info_text[k].term, NULL, NULL) == 0) {
      print_info (f, prompt, k);
      n++;
    }
  }
  if (n > 0) {
    return;
  }
  for (k = 0; info_text[k].cat != NULL; k++) {
    if (grep_in_string (item, info_text[k].def, NULL, NULL) == 0) {
      print_info (f, prompt, k);
    }
  }
}
