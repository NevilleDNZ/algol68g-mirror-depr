//! @file sounds.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
//
//! @section License
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 3 of the License, or 
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
// more details. You should have received a copy of the GNU General Public 
// License along with this program. If not, see <http://www.gnu.org/licenses/>.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-physics.h"
#include "a68g-numbers.h"
#include "a68g-optimiser.h"
#include "a68g-double.h"

// Implementation of SOUND values.

#define MAX_BYTES 4
#define A68_LITTLE_ENDIAN A68_TRUE
#define A68_BIG_ENDIAN A68_FALSE

// From public Microsoft RIFF documentation.

#define	WAVE_FORMAT_UNKNOWN		(0x0000)
#define	WAVE_FORMAT_PCM			(0x0001)
#define	WAVE_FORMAT_ADPCM		(0x0002)
#define WAVE_FORMAT_IEEE_FLOAT          (0x0003)
#define WAVE_FORMAT_IBM_FORMAT_CVSD	(0x0005)
#define	WAVE_FORMAT_ALAW		(0x0006)
#define	WAVE_FORMAT_MULAW		(0x0007)
#define	WAVE_FORMAT_OKI_ADPCM		(0x0010)
#define WAVE_FORMAT_DVI_ADPCM		(0x0011)
#define WAVE_FORMAT_MEDIASPACE_ADPCM	(0x0012)
#define WAVE_FORMAT_SIERRA_ADPCM	(0x0013)
#define WAVE_FORMAT_G723_ADPCM		(0X0014)
#define	WAVE_FORMAT_DIGISTD		(0x0015)
#define	WAVE_FORMAT_DIGIFIX		(0x0016)
#define WAVE_FORMAT_YAMAHA_ADPCM	(0x0020)
#define WAVE_FORMAT_SONARC		(0x0021)
#define WAVE_FORMAT_DSPGROUP_TRUESPEECH	(0x0022)
#define WAVE_FORMAT_ECHOSCI1		(0x0023)
#define WAVE_FORMAT_AUDIOFILE_AF36	(0x0024)
#define WAVE_FORMAT_APTX		(0x0025)
#define WAVE_FORMAT_AUDIOFILE_AF10	(0x0026)
#define WAVE_FORMAT_DOLBY_AC2           (0x0030)
#define WAVE_FORMAT_GSM610              (0x0031)
#define WAVE_FORMAT_ANTEX_ADPCME	(0x0033)
#define WAVE_FORMAT_CONTROL_RES_VQLPC	(0x0034)
#define WAVE_FORMAT_DIGIREAL		(0x0035)
#define WAVE_FORMAT_DIGIADPCM		(0x0036)
#define WAVE_FORMAT_CONTROL_RES_CR10	(0x0037)
#define WAVE_FORMAT_NMS_VBXADPCM	(0x0038)
#define WAVE_FORMAT_ROCKWELL_ADPCM      (0x003b)
#define WAVE_FORMAT_ROCKWELL_DIGITALK   (0x003c)
#define WAVE_FORMAT_G721_ADPCM          (0x0040)
#define WAVE_FORMAT_G728_CELP           (0x0041)
#define WAVE_FORMAT_MPEG                (0x0050)
#define WAVE_FORMAT_MPEGLAYER3          (0x0055)
#define WAVE_FORMAT_G726_ADPCM          (0x0064)
#define WAVE_FORMAT_G722_ADPCM          (0x0065)
#define WAVE_FORMAT_IBM_FORMAT_MULAW	(0x0101)
#define WAVE_FORMAT_IBM_FORMAT_ALAW	(0x0102)
#define WAVE_FORMAT_IBM_FORMAT_ADPCM	(0x0103)
#define WAVE_FORMAT_CREATIVE_ADPCM	(0x0200)
#define WAVE_FORMAT_FM_TOWNS_SND	(0x0300)
#define WAVE_FORMAT_OLIGSM		(0x1000)
#define WAVE_FORMAT_OLIADPCM		(0x1001)
#define WAVE_FORMAT_OLICELP		(0x1002)
#define WAVE_FORMAT_OLISBC		(0x1003)
#define WAVE_FORMAT_OLIOPR		(0x1004)
#define WAVE_FORMAT_EXTENSIBLE          (0xfffe)

static unsigned pow256[] = { 1, 256, 65536, 16777216 };

//! @brief Test bits per sample.

static void test_bits_per_sample (NODE_T * p, unsigned bps)
{
  if (bps <= 0 || bps > 24) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "unsupported number of bits per sample");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief Code string into big-endian unsigned.

static unsigned code_string (NODE_T * p, char *s, int n)
{
  unsigned v;
  int k, m;
  if (n > MAX_BYTES) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "too long word length");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (k = 0, m = n - 1, v = 0; k < n; k++, m--) {
    v += ((unsigned) s[k]) * pow256[m];
  } return v;
}

//! @brief Code unsigned into string.

static char *code_unsigned (NODE_T * p, unsigned n)
{
  static char text[MAX_BYTES + 1];
  int k;
  (void) p;
  for (k = 0; k < MAX_BYTES; k++) {
    char ch = (char) (n % 0x100);
    if (ch == NULL_CHAR) {
      ch = BLANK_CHAR;
    } else if (ch < BLANK_CHAR) {
      ch = '?';
    }
    text[MAX_BYTES - k - 1] = ch;
    n >>= 8;
  }
  text[MAX_BYTES] = NULL_CHAR;
  return text;
}

//! @brief WAVE format category

static char *format_category (unsigned n)
{
  switch (n) {
  case WAVE_FORMAT_UNKNOWN:
    {
      return "WAVE_FORMAT_UNKNOWN";
    }
  case WAVE_FORMAT_PCM:
    {
      return "WAVE_FORMAT_PCM	";
    }
  case WAVE_FORMAT_ADPCM:
    {
      return "WAVE_FORMAT_ADPCM";
    }
  case WAVE_FORMAT_IEEE_FLOAT:
    {
      return "WAVE_FORMAT_IEEE_FLOAT";
    }
  case WAVE_FORMAT_IBM_FORMAT_CVSD:
    {
      return "WAVE_FORMAT_IBM_FORMAT_CVSD";
    }
  case WAVE_FORMAT_ALAW:
    {
      return "WAVE_FORMAT_ALAW";
    }
  case WAVE_FORMAT_MULAW:
    {
      return "WAVE_FORMAT_MULAW";
    }
  case WAVE_FORMAT_OKI_ADPCM:
    {
      return "WAVE_FORMAT_OKI_ADPCM";
    }
  case WAVE_FORMAT_DVI_ADPCM:
    {
      return "WAVE_FORMAT_DVI_ADPCM";
    }
  case WAVE_FORMAT_MEDIASPACE_ADPCM:
    {
      return "WAVE_FORMAT_MEDIASPACE_ADPCM";
    }
  case WAVE_FORMAT_SIERRA_ADPCM:
    {
      return "WAVE_FORMAT_SIERRA_ADPCM";
    }
  case WAVE_FORMAT_G723_ADPCM:
    {
      return "WAVE_FORMAT_G723_ADPCM";
    }
  case WAVE_FORMAT_DIGISTD:
    {
      return "WAVE_FORMAT_DIGISTD";
    }
  case WAVE_FORMAT_DIGIFIX:
    {
      return "WAVE_FORMAT_DIGIFIX";
    }
  case WAVE_FORMAT_YAMAHA_ADPCM:
    {
      return "WAVE_FORMAT_YAMAHA_ADPCM";
    }
  case WAVE_FORMAT_SONARC:
    {
      return "WAVE_FORMAT_SONARC";
    }
  case WAVE_FORMAT_DSPGROUP_TRUESPEECH:
    {
      return "WAVE_FORMAT_DSPGROUP_TRUESPEECH";
    }
  case WAVE_FORMAT_ECHOSCI1:
    {
      return "WAVE_FORMAT_ECHOSCI1";
    }
  case WAVE_FORMAT_AUDIOFILE_AF36:
    {
      return "WAVE_FORMAT_AUDIOFILE_AF36";
    }
  case WAVE_FORMAT_APTX:
    {
      return "WAVE_FORMAT_APTX";
    }
  case WAVE_FORMAT_AUDIOFILE_AF10:
    {
      return "WAVE_FORMAT_AUDIOFILE_AF10";
    }
  case WAVE_FORMAT_DOLBY_AC2:
    {
      return "WAVE_FORMAT_DOLBY_AC2";
    }
  case WAVE_FORMAT_GSM610:
    {
      return "WAVE_FORMAT_GSM610 ";
    }
  case WAVE_FORMAT_ANTEX_ADPCME:
    {
      return "WAVE_FORMAT_ANTEX_ADPCME";
    }
  case WAVE_FORMAT_CONTROL_RES_VQLPC:
    {
      return "WAVE_FORMAT_CONTROL_RES_VQLPC";
    }
  case WAVE_FORMAT_DIGIREAL:
    {
      return "WAVE_FORMAT_DIGIREAL";
    }
  case WAVE_FORMAT_DIGIADPCM:
    {
      return "WAVE_FORMAT_DIGIADPCM";
    }
  case WAVE_FORMAT_CONTROL_RES_CR10:
    {
      return "WAVE_FORMAT_CONTROL_RES_CR10";
    }
  case WAVE_FORMAT_NMS_VBXADPCM:
    {
      return "WAVE_FORMAT_NMS_VBXADPCM";
    }
  case WAVE_FORMAT_ROCKWELL_ADPCM:
    {
      return "WAVE_FORMAT_ROCKWELL_ADPCM";
    }
  case WAVE_FORMAT_ROCKWELL_DIGITALK:
    {
      return "WAVE_FORMAT_ROCKWELL_DIGITALK";
    }
  case WAVE_FORMAT_G721_ADPCM:
    {
      return "WAVE_FORMAT_G721_ADPCM";
    }
  case WAVE_FORMAT_G728_CELP:
    {
      return "WAVE_FORMAT_G728_CELP";
    }
  case WAVE_FORMAT_MPEG:
    {
      return "WAVE_FORMAT_MPEG";
    }
  case WAVE_FORMAT_MPEGLAYER3:
    {
      return "WAVE_FORMAT_MPEGLAYER3";
    }
  case WAVE_FORMAT_G726_ADPCM:
    {
      return "WAVE_FORMAT_G726_ADPCM";
    }
  case WAVE_FORMAT_G722_ADPCM:
    {
      return "WAVE_FORMAT_G722_ADPCM";
    }
  case WAVE_FORMAT_IBM_FORMAT_MULAW:
    {
      return "WAVE_FORMAT_IBM_FORMAT_MULAW";
    }
  case WAVE_FORMAT_IBM_FORMAT_ALAW:
    {
      return "WAVE_FORMAT_IBM_FORMAT_ALAW";
    }
  case WAVE_FORMAT_IBM_FORMAT_ADPCM:
    {
      return "WAVE_FORMAT_IBM_FORMAT_ADPCM";
    }
  case WAVE_FORMAT_CREATIVE_ADPCM:
    {
      return "WAVE_FORMAT_CREATIVE_ADPCM";
    }
  case WAVE_FORMAT_FM_TOWNS_SND:
    {
      return "WAVE_FORMAT_FM_TOWNS_SND";
    }
  case WAVE_FORMAT_OLIGSM:
    {
      return "WAVE_FORMAT_OLIGSM";
    }
  case WAVE_FORMAT_OLIADPCM:
    {
      return "WAVE_FORMAT_OLIADPCM";
    }
  case WAVE_FORMAT_OLICELP:
    {
      return "WAVE_FORMAT_OLICELP";
    }
  case WAVE_FORMAT_OLISBC:
    {
      return "WAVE_FORMAT_OLISBC";
    }
  case WAVE_FORMAT_OLIOPR:
    {
      return "WAVE_FORMAT_OLIOPR";
    }
  case WAVE_FORMAT_EXTENSIBLE:
    {
      return "WAVE_FORMAT_EXTENSIBLE";
    }
  default:
    {
      return "other";
    }
  }
}

//! @brief Read RIFF item.

static unsigned read_riff_item (NODE_T * p, FILE_T fd, int n, BOOL_T little)
{
  unsigned v, z;
  int k, m, r;
  if (n > MAX_BYTES) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "too long word length");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (little) {
    for (k = 0, m = 0, v = 0; k < n; k++, m++) {
      z = 0;
      r = (int) io_read (fd, &z, (size_t) 1);
      if (r != 1 || errno != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "error while reading file");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      v += z * pow256[m];
    }
  } else {
    for (k = 0, m = n - 1, v = 0; k < n; k++, m--) {
      z = 0;
      r = (int) io_read (fd, &z, (size_t) 1);
      if (r != 1 || errno != 0) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "error while reading file");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      v += z * pow256[m];
    }
  }
  return v;
}

//! @brief Read sound from file.

void read_sound (NODE_T * p, A68_REF ref_file, A68_SOUND * w)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int r;
  unsigned fmt_cat;
  unsigned blockalign, byterate, chunksize, subchunk2size, z;
  BOOL_T data_read = A68_FALSE;
  if (read_riff_item (p, FD (f), 4, A68_BIG_ENDIAN) != code_string (p, "RIFF", 4)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "file format is not RIFF");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  chunksize = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
  if ((z = read_riff_item (p, FD (f), 4, A68_BIG_ENDIAN)) != code_string (p, "WAVE", 4)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL_STRING, M_SOUND, "file format is not \"WAVE\" but", code_unsigned (p, z));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
// Now read chunks.
  while (data_read == A68_FALSE) {
    z = read_riff_item (p, FD (f), 4, A68_BIG_ENDIAN);
    if (z == code_string (p, "fmt ", 4)) {
// Read fmt chunk.
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z - 0x10;    // Bytes to skip in extended wave format
      fmt_cat = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      if (fmt_cat != WAVE_FORMAT_PCM) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL_STRING, M_SOUND, "category is not WAVE_FORMAT_PCM but", format_category (fmt_cat));
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      NUM_CHANNELS (w) = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      SAMPLE_RATE (w) = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      byterate = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      blockalign = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      BITS_PER_SAMPLE (w) = read_riff_item (p, FD (f), 2, A68_LITTLE_ENDIAN);
      test_bits_per_sample (p, BITS_PER_SAMPLE (w));
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "LIST", 4)) {
// Skip a LIST chunk.
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z;
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "cue ", 4)) {
// Skip a cue chunk.
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z;
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "fact", 4)) {
// Skip a fact chunk.
      int k, skip;
      z = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      skip = (int) z;
      for (k = 0; k < skip; k++) {
        z = read_riff_item (p, FD (f), 1, A68_LITTLE_ENDIAN);
      }
    } else if (z == code_string (p, "data", 4)) {
// Read data chunk.
      subchunk2size = read_riff_item (p, FD (f), 4, A68_LITTLE_ENDIAN);
      NUM_SAMPLES (w) = subchunk2size / NUM_CHANNELS (w) / (unsigned) A68_SOUND_BYTES (w);
      DATA (w) = heap_generator (p, M_SOUND_DATA, (int) subchunk2size);
      r = (int) io_read (FD (f), ADDRESS (&(DATA (w))), subchunk2size);
      if (r != (int) subchunk2size) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "cannot read all of the data");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
      data_read = A68_TRUE;
    } else {
      diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL_STRING, M_SOUND, "chunk is", code_unsigned (p, z));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  (void) blockalign;
  (void) byterate;
  (void) chunksize;
  (void) subchunk2size;
  STATUS (w) = INIT_MASK;
}

//! @brief Write RIFF item.

void write_riff_item (NODE_T * p, FILE_T fd, unsigned z, int n, BOOL_T little)
{
  int k, r;
  unsigned char y[MAX_BYTES];
  if (n > MAX_BYTES) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "too long word length");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (k = 0; k < n; k++) {
    y[k] = (unsigned char) (z & 0xff);
    z >>= 8;
  }
  if (little) {
    for (k = 0; k < n; k++) {
      ASSERT (io_write (fd, &(y[k]), 1) != -1);
    }
  } else {
    for (k = n - 1; k >= 0; k--) {
      r = (int) io_write (fd, &(y[k]), 1);
      if (r != 1) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "error while writing file");
        exit_genie (p, A68_RUNTIME_ERROR);
      }
    }
  }
}

//! @brief Write sound to file.

void write_sound (NODE_T * p, A68_REF ref_file, A68_SOUND * w)
{
  A68_FILE *f = FILE_DEREF (&ref_file);
  int r;
  unsigned blockalign = NUM_CHANNELS (w) * (unsigned) (A68_SOUND_BYTES (w));
  unsigned byterate = SAMPLE_RATE (w) * blockalign;
  unsigned subchunk2size = NUM_SAMPLES (w) * blockalign;
  unsigned chunksize = 4 + (8 + 16) + (8 + subchunk2size);
  write_riff_item (p, FD (f), code_string (p, "RIFF", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), chunksize, 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), code_string (p, "WAVE", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), code_string (p, "fmt ", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), 16, 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), 1, 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), NUM_CHANNELS (w), 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), SAMPLE_RATE (w), 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), byterate, 4, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), blockalign, 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), BITS_PER_SAMPLE (w), 2, A68_LITTLE_ENDIAN);
  write_riff_item (p, FD (f), code_string (p, "data", 4), 4, A68_BIG_ENDIAN);
  write_riff_item (p, FD (f), subchunk2size, 4, A68_LITTLE_ENDIAN);
  if (IS_NIL (DATA (w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "sound has no data");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  r = (int) io_write (FD (f), ADDRESS (&(DATA (w))), subchunk2size);
  if (r != (int) subchunk2size) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "error while writing file");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

//! @brief PROC new sound = (INT bits, INT sample rate, INT channels, INT samples) SOUND

void genie_new_sound (NODE_T * p)
{
  A68_INT num_channels, sample_rate, bits_per_sample, num_samples;
  A68_SOUND w;
  POP_OBJECT (p, &num_samples, A68_INT);
  POP_OBJECT (p, &num_channels, A68_INT);
  POP_OBJECT (p, &sample_rate, A68_INT);
  POP_OBJECT (p, &bits_per_sample, A68_INT);
  NUM_SAMPLES (&w) = (unsigned) (VALUE (&num_samples));
  NUM_CHANNELS (&w) = (unsigned) (VALUE (&num_channels));
  SAMPLE_RATE (&w) = (unsigned) (VALUE (&sample_rate));
  BITS_PER_SAMPLE (&w) = (unsigned) (VALUE (&bits_per_sample));
  test_bits_per_sample (p, BITS_PER_SAMPLE (&w));
  DATA_SIZE (&w) = (unsigned) A68_SOUND_DATA_SIZE (&w);
  DATA (&w) = heap_generator (p, M_SOUND_DATA, (int) DATA_SIZE (&w));
  STATUS (&w) = INIT_MASK;
  PUSH_OBJECT (p, w, A68_SOUND);
}

//! @brief PROC get sound = (SOUND w, INT channel, sample) INT

void genie_get_sound (NODE_T * p)
{
  A68_INT channel, sample;
  A68_SOUND w;
  int addr, k, n, z, m;
  BYTE_T *d;
  POP_OBJECT (p, &sample, A68_INT);
  POP_OBJECT (p, &channel, A68_INT);
  POP_OBJECT (p, &w, A68_SOUND);
  if (!(VALUE (&channel) >= 1 && VALUE (&channel) <= (int) NUM_CHANNELS (&w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "channel index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!(VALUE (&sample) >= 1 && VALUE (&sample) <= (int) NUM_SAMPLES (&w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "sample index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (IS_NIL (DATA (&w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "sound has no data");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  n = A68_SOUND_BYTES (&w);
  addr = ((VALUE (&sample) - 1) * (int) (NUM_CHANNELS (&w)) + (VALUE (&channel) - 1)) * n;
  ABEND (addr < 0 || addr >= (int) DATA_SIZE (&w), ERROR_INTERNAL_CONSISTENCY, __func__);
  d = &(ADDRESS (&(DATA (&w)))[addr]);
// Convert from little-endian, irrespective from the platform we work on.
  for (k = 0, z = 0, m = 0; k < n; k++) {
    z += ((int) (d[k]) * (int) (pow256[k]));
    m = k;
  }
  PUSH_VALUE (p, (d[m] & 0x80 ? (n == 4 ? z : z - (int) pow256[m + 1]) : z), A68_INT);
}

//! @brief PROC set sound = (SOUND w, INT channel, sample, value) VOID

void genie_set_sound (NODE_T * p)
{
  A68_INT channel, sample, value;
  int addr, k, n, z;
  BYTE_T *d;
  A68_SOUND w;
  POP_OBJECT (p, &value, A68_INT);
  POP_OBJECT (p, &sample, A68_INT);
  POP_OBJECT (p, &channel, A68_INT);
  POP_OBJECT (p, &w, A68_SOUND);
  if (!(VALUE (&channel) >= 1 && VALUE (&channel) <= (int) NUM_CHANNELS (&w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "channel index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (!(VALUE (&sample) >= 1 && VALUE (&sample) <= (int) NUM_SAMPLES (&w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "sample index out of range");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (IS_NIL (DATA (&w))) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_SOUND_INTERNAL, M_SOUND, "sound has no data");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  n = A68_SOUND_BYTES (&w);
  addr = ((VALUE (&sample) - 1) * (int) (NUM_CHANNELS (&w)) + (VALUE (&channel) - 1)) * n;
  ABEND (addr < 0 || addr >= (int) DATA_SIZE (&w), ERROR_INTERNAL_CONSISTENCY, __func__);
  d = &(ADDRESS (&(DATA (&w)))[addr]);
// Convert to little-endian.
  for (k = 0, z = VALUE (&value); k < n; k++) {
    d[k] = (BYTE_T) (z & 0xff);
    z >>= 8;
  }
}

//! @brief OP SOUND = (SOUND) INT

void genie_sound_samples (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_VALUE (p, (int) (NUM_SAMPLES (&w)), A68_INT);
}

//! @brief OP RATE = (SOUND) INT

void genie_sound_rate (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_VALUE (p, (int) (SAMPLE_RATE (&w)), A68_INT);
}

//! @brief OP CHANNELS = (SOUND) INT

void genie_sound_channels (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_VALUE (p, (int) (NUM_CHANNELS (&w)), A68_INT);
}

//! @brief OP RESOLUTION = (SOUND) INT

void genie_sound_resolution (NODE_T * p)
{
  A68_SOUND w;
  POP_OBJECT (p, &w, A68_SOUND);
  PUSH_VALUE (p, (int) (BITS_PER_SAMPLE (&w)), A68_INT);
}
