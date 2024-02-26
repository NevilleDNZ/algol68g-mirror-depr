//! @file plugin-script.c
//! @author J. Marcel van der Veer
//!
//! @section Copyright
//!
//! This file is part of Algol68G - an Algol 68 compiler-interpreter.
//! Copyright 2001-2023 J. Marcel van der Veer [algol68g@xs4all.nl].
//!
//! @section License
//!
//! This program is free software; you can redistribute it and/or modify it 
//! under the terms of the GNU General Public License as published by the 
//! Free Software Foundation; either version 3 of the License, or 
//! (at your option) any later version.
//!
//! This program is distributed in the hope that it will be useful, but 
//! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
//! or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
//! more details. You should have received a copy of the GNU General Public 
//! License along with this program. If not, see [http://www.gnu.org/licenses/].

//! @section Synopsis
//!
//! Plugin script builder routines.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-optimiser.h"
#include "a68g-genie.h"
#include "a68g-options.h"

#if defined (BUILD_A68_COMPILER)

//! @brief Build shell script from program.

void build_script (void)
{
  int ret;
  FILE_T script, source;
  LINE_T *sl;
  BUFFER cmd;
  BUFCLR (cmd);
  char *strop;
#if !defined (BUILD_A68_COMPILER)
  return;
#endif
  announce_phase ("script builder");
  ABEND (OPTION_OPT_LEVEL (&A68_JOB) == 0, ERROR_ACTION, __func__);
// Flatten the source file.
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SOURCE_NAME (&A68_JOB)) >= 0);
  source = open (cmd, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (source == -1, ERROR_ACTION, cmd);
  for (sl = TOP_LINE (&A68_JOB); sl != NO_LINE; FORWARD (sl)) {
    if (strlen (STRING (sl)) == 0 || (STRING (sl))[strlen (STRING (sl)) - 1] != NEWLINE_CHAR) {
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s\n%d\n%s\n", FILENAME (sl), NUMBER (sl), STRING (sl)) >= 0);
    } else {
      ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s\n%d\n%s", FILENAME (sl), NUMBER (sl), STRING (sl)) >= 0);
    }
    WRITE (source, cmd);
  }
  ASSERT (close (source) == 0);
// Compress source and dynamic library.
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "cp %s %s.%s", FILE_PLUGIN_NAME (&A68_JOB), HIDDEN_TEMP_FILE_NAME, FILE_PLUGIN_NAME (&A68_JOB)) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "tar czf %s.%s.tgz %s.%s %s.%s", HIDDEN_TEMP_FILE_NAME, FILE_GENERIC_NAME (&A68_JOB), HIDDEN_TEMP_FILE_NAME, FILE_SOURCE_NAME (&A68_JOB), HIDDEN_TEMP_FILE_NAME, FILE_PLUGIN_NAME (&A68_JOB)) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
// Compose script.
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SCRIPT_NAME (&A68_JOB)) >= 0);
  script = open (cmd, O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (script == -1, ERROR_ACTION, cmd);
  strop = "";
  if (OPTION_STROPPING (&A68_JOB) == QUOTE_STROPPING) {
    strop = "--run-quote-script";
  } else {
    strop = "--run-script";
  }
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "#! %s/a68g %s\n", BINDIR, strop) >= 0);
  WRITE (script, A68 (output_line));
  ASSERT (snprintf (A68 (output_line), SNPRINTF_SIZE, "%s\n%s --verify \"%s\"\n", FILE_GENERIC_NAME (&A68_JOB), optimisation_option (), PACKAGE_STRING) >= 0);
  WRITE (script, A68 (output_line));
  ASSERT (close (script) == 0);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "cat %s.%s %s.%s.tgz > %s", HIDDEN_TEMP_FILE_NAME, FILE_SCRIPT_NAME (&A68_JOB), HIDDEN_TEMP_FILE_NAME, FILE_GENERIC_NAME (&A68_JOB), FILE_SCRIPT_NAME (&A68_JOB)) >= 0);
  ret = system (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s", FILE_SCRIPT_NAME (&A68_JOB)) >= 0);
  ret = chmod (cmd, (__mode_t) (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH));  // -rwx-r-xr-x
  ABEND (ret != 0, ERROR_ACTION, cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
// Clean up.
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s.tgz", HIDDEN_TEMP_FILE_NAME, FILE_GENERIC_NAME (&A68_JOB)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SOURCE_NAME (&A68_JOB)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_PLUGIN_NAME (&A68_JOB)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, FILE_SCRIPT_NAME (&A68_JOB)) >= 0);
  ret = remove (cmd);
  ABEND (ret != 0, ERROR_ACTION, cmd);
}

//! @brief Load program from shell script .

void load_script (void)
{
  int k; FILE_T script; BUFFER cmd; char ch;
  BUFCLR (cmd);
#if !defined (BUILD_A68_COMPILER)
  return;
#endif
  announce_phase ("script loader");
// Decompress the archive.
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "sed '1,3d' < %s | tar xzf -", FILE_INITIAL_NAME (&A68_JOB)) >= 0);
  ABEND (system (cmd) != 0, ERROR_ACTION, cmd);
// Reread the header.
  script = open (FILE_INITIAL_NAME (&A68_JOB), O_RDONLY);
  ABEND (script == -1, ERROR_ACTION, cmd);
// Skip the #! a68g line.
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    ASSERT (io_read (script, &ch, 1) == 1);
  }
// Read the generic filename.
  A68 (input_line)[0] = NULL_CHAR;
  k = 0;
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    A68 (input_line)[k++] = ch;
    ASSERT (io_read (script, &ch, 1) == 1);
  }
  A68 (input_line)[k] = NULL_CHAR;
  ASSERT (snprintf (cmd, SNPRINTF_SIZE, "%s.%s", HIDDEN_TEMP_FILE_NAME, A68 (input_line)) >= 0);
  FILE_INITIAL_NAME (&A68_JOB) = new_string (cmd, NO_TEXT);
// Read options.
  A68 (input_line)[0] = NULL_CHAR;
  k = 0;
  ASSERT (io_read (script, &ch, 1) == 1);
  while (ch != NEWLINE_CHAR) {
    A68 (input_line)[k++] = ch;
    ASSERT (io_read (script, &ch, 1) == 1);
  }
  isolate_options (A68 (input_line), NO_LINE);
  (void) set_options (OPTION_LIST (&A68_JOB), A68_FALSE);
  ASSERT (close (script) == 0);
}

//! @brief Rewrite source for shell script .

void rewrite_script_source (void)
{
  LINE_T *ref_l = NO_LINE;
  FILE_T source;
// Rebuild the source file.
  ASSERT (remove (FILE_SOURCE_NAME (&A68_JOB)) == 0);
  source = open (FILE_SOURCE_NAME (&A68_JOB), O_WRONLY | O_CREAT | O_TRUNC, A68_PROTECTION);
  ABEND (source == -1, ERROR_ACTION, FILE_SOURCE_NAME (&A68_JOB));
  for (ref_l = TOP_LINE (&A68_JOB); ref_l != NO_LINE; FORWARD (ref_l)) {
    WRITE (source, STRING (ref_l));
    if (strlen (STRING (ref_l)) == 0 || (STRING (ref_l))[strlen (STRING (ref_l) - 1)] != NEWLINE_CHAR) {
      WRITE (source, NEWLINE_STRING);
    }
  }
// Wrap it up.
  ASSERT (close (source) == 0);
}

#endif
