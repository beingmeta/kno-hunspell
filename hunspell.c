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

#define kno_hunspeller_type 0x888
static lispval hunspeller_typetag;

struct KNO_HUNSPELLER {
  Hunhandle *hun_handle;
  u8_text_encoding dict_encoding;} *kno_hunspeller;

u8_string default_hunspell_dir;

KNO_EXPORT int kno_init_hunspell(void) KNO_LIBINIT_FN;

static void recycle_hunspeller(void *ptr)
{
  struct KNO_HUNSPELLER *kh = (kno_hunspeller) ptr;
  Hunspell_destroy(kh->hun_handle);
  u8_free(kh);
}

static lispval hunspell_open(lispval path,lispval opts,lispval custom)
{
  lispval prefix = KNO_CSTRING(path);
  u8_string affpath = u8_string_append(prefix,".aff",NULL);
  u8_string dicpath = u8_string_append(prefix,".dic",NULL);
  lispval keyopt = kno_getopt(opts,KNOSYM_KEY,KNO_VOID);
  char *dict_key = (KNO_PACKETP(keyopt)) ? (KNO_PACKET_DATA(keypot)) :
    (KNO_STRINGP(keyopt)) ? (KNO_CSTRING(keypot)) : (NULL);
  Hunhandle *h = (dictkey) ?
    (Hunspell_create_key(affpath,dicpath,dict_key)) :
    (Hunspell_create(affpath,dpath));
  if (h == NULL) {}
  char *encname = Hunspell_get_dic_encoding(h);
  u8_text_encoding enc = u8_get_encoding(encname);
  struct KNO_HUNSPELLER *kh = u8_alloc(struct KNO_HUNSPELLER);
  kh->hun_handle = h;
  kh->dict_encoding = enc;
  return kno_wrap_pointer(kh,sizeof(struct KNO_HUNSPELLER),
			  recycle_hunspeller,
			  hunspeller_typetag,
			  id);
}

static lispval convert_stringlist(u8_encoding enc,
				  int n,char ***strings,
				  int as_choice)
{
  lispval each[n], result;
  int i = 0; while (i<n) {
    char *string = strings[i];
    if (enc) {
      u8_string converted = u8_make_string(enc,strings[i],NULL);
      each[i]=kno_init_string(NULL,-1,converted);}
    else each[i]=knostring(strings[i]);
    i++;}
  if (as_vec)
    return kno_make_vector(n,each);
  else return kno_make_choice(n,each,KNO_CHOICE_ISCONSES|KNO_CHOICE_DOSORT);
}



/* suggest(suggestions, word) - search suggestions
 * input: pointer to an array of strings pointer and the (bad) word
 *   array of strings pointer (here *slst) may not be initialized
 * output: number of suggestions in string array, and suggestions in
 *   a newly allocated array of strings (*slts will be NULL when number
 *   of suggestion equals 0.)
 */
LIBHUNSPELL_DLL_EXPORTED int Hunspell_suggest(Hunhandle* pHunspell,
                                              char*** slst,
                                              const char* word);

/* morphological functions */

/* analyze(result, word) - morphological analysis of the word */

LIBHUNSPELL_DLL_EXPORTED int Hunspell_analyze(Hunhandle* pHunspell,
                                              char*** slst,
                                              const char* word);

/* stem(result, word) - stemmer function */

LIBHUNSPELL_DLL_EXPORTED int Hunspell_stem(Hunhandle* pHunspell,
                                           char*** slst,
                                           const char* word);

/* stem(result, analysis, n) - get stems from a morph. analysis
 * example:
 * char ** result, result2;
 * int n1 = Hunspell_analyze(result, "words");
 * int n2 = Hunspell_stem2(result2, result, n1);
 */

LIBHUNSPELL_DLL_EXPORTED int Hunspell_stem2(Hunhandle* pHunspell,
                                            char*** slst,
                                            char** desc,
                                            int n);

/* generate(result, word, word2) - morphological generation by example(s) */

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
