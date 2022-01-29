/* -*- Mode: C; Character-encoding: utf-8; -*- */

/* Copyright (C) 2012-2019 beingmeta, inc.
   Copyright (C) 2020-2022 beingmeta, LLC
   This file is part of beingmeta's Kno platform and is copyright
   and a valuable trade secret of beingmeta, inc.
*/

#ifndef _FILEINFO
#define _FILEINFO __FILE__
#endif

#include "kno/knosource.h"
#include "kno/lisp.h"
#include "kno/eval.h"
#include "kno/storage.h"
#include "kno/pools.h"
#include "kno/indexes.h"
#include "kno/frames.h"
#include "kno/numbers.h"
#include "kno/cprims.h"

#include <libu8/libu8io.h>
#include <libu8/u8pathfns.h>
#include <libu8/u8filefns.h>
#include <hunspell.h>

u8_string default_hunspell_dir;

KNO_EXPORT int kno_init_hunspell(void) KNO_LIBINIT_FN;

static lispval hunspell_module;

KNO_EXPORT int kno_init_hunspell()
{
  if (hunspell_init) return 0;

  hunspell_init = u8_millitime();

  kno_register_config("HUNSPELL:DATA","Where the hunspell module gets its data",
		      kno_sconfig_get,kno_sconfig_set,
		      &default_hunspell_dir);

  if (default_hyphenation_file)
    default_dict = hnj_hyphen_load(default_hyphenation_file);
  else {
    u8_string dictfile = u8_mkpath(KNO_DATA_DIR,"hyph_en_US.dic");
    if (u8_file_existsp(dictfile))
      default_dict = hnj_hyphen_load(dictfile);
    else {
      u8_log(LOG_CRIT,kno_FileNotFound,
	     "Hyphenation dictionary %s does not exist!",
	     dictfile);}
    u8_free(dictfile);}

  hunspell_module = kno_new_cmodule("hunspell",0,kno_init_hunspell);

  link_local_cprims();

  kno_finish_module(hunspell_module);

  u8_register_source_file(_FILEINFO);

  return 1;
}

static void link_local_cprims()
{
}
