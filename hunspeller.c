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

#define kno_hunspeller_type 0x13913c7e

static lispval hunspeller_typetag;

struct KNO_TYPEINFO *kno_hunspeller_typeinfo;

u8_condition HunspellError=_("Hunspell library error");

typedef struct KNO_HUNSPELLER {
  Hunhandle *c_handle;
  u8_string dict_path;
  struct U8_TEXT_ENCODING *dict_encoding;} *kno_hunspeller;

u8_string default_hunspell_dir;

#ifndef CSTRING
#define CSTRING KNO_CSTRING
#endif
#ifndef CSTRLEN
#define CSTRLEN KNO_STRLEN
#endif
#ifndef PNAME
#define PNAME KNO_SYMBOL_NAME
#endif

KNO_EXPORT int kno_init_hunspeller(void) KNO_LIBINIT_FN;

static void recycle_hunspeller(void *ptr)
{
  struct KNO_HUNSPELLER *kh = (kno_hunspeller) ptr;
  Hunspell_destroy(kh->c_handle);
  if (kh->dict_path) u8_free(kh->dict_path);
  u8_free(kh);
}

u8_string get_hunspell_prefix(lispval arg,lispval opts)
{
  if (KNO_STRINGP(arg))
    return u8_strdup(CSTRING(arg));
  else if (KNO_SYMBOLP(arg)) {
    lispval config_val = kno_config_get(PNAME(arg));
    if (VOIDP(config_val)) return NULL;
    else if (STRINGP(config_val))
      return u8_strdup(CSTRING(arg));
    else {
      u8_log(LOGWARN,"BadConfigPath",
	     "Bad config value %q for hunspell path spec %q",
	     config_val,arg);
      kno_decref(config_val);
      return NULL;}}
  else return NULL;
}

DEFC_PRIM("hunspell/open",hunspell_open_prim,
	  KNO_MAX_ARGS(3)|KNO_MIN_ARGS(1),
	  "Opens a hunspell analyzer for a particular dictionary",
	  {"path",kno_any_type,KNO_VOID},
	  {"opts",kno_opts_type,KNO_FALSE},
	  {"custom",kno_any_type,KNO_FALSE})
static lispval hunspell_open_prim(lispval path,lispval opts,lispval custom)
{
  u8_string prefix = get_hunspell_prefix(path,opts);
  if (prefix == NULL)
    return kno_err(HunspellError,"hunspell_open_prim",NULL,path);
  u8_string affpath = u8_string_append(prefix,".aff",NULL);
  u8_string dicpath = u8_string_append(prefix,".dic",NULL);
  lispval keyopt = kno_getopt(opts,KNOSYM_KEY,KNO_VOID);
  const char *dict_key = (KNO_PACKETP(keyopt)) ? (KNO_PACKET_DATA(keyopt)) :
    (KNO_STRINGP(keyopt)) ? (KNO_CSTRING(keyopt)) : (NULL);
  Hunhandle *h = (dict_key) ?
    (Hunspell_create_key(affpath,dicpath,dict_key)) :
    (Hunspell_create(affpath,dicpath));
  if (h == NULL) {
    kno_decref(keyopt);
    return kno_err(HunspellError,"hunspell_open_prim",CSTRING(path),opts);}

  char *encname = Hunspell_get_dic_encoding(h);
  u8_encoding enc = NULL;
  if (encname == NULL) {}
  else if ( (strcasecmp(encname,"utf-8")==0) || (strcasecmp(encname,"utf8")==0) )
    enc=NULL;
  else enc=u8_get_encoding(encname);

  if (!( (KNO_VOIDP(custom)) || (KNO_FALSEP(custom)) || (KNO_DEFAULTP(custom)) )) {
    // TODO: Add type warnings
    KNO_DO_CHOICES(add,custom) {
      if (KNO_STRINGP(add)) {
	int rv;
	if (enc) {
	  const char *dict_string =
	    u8_localize_string(enc,CSTRING(add),CSTRING(add)+CSTRLEN(add));
	  rv=Hunspell_add(h,dict_string);}
	else rv=Hunspell_add(h,(const char *) u8_strdup(CSTRING(add)));
	if (rv<0) u8_log(LOGWARN,"Hunspell/AddFailed",prefix,add);}
      else if (KNO_TABLEP(add)) {
	lispval keys = kno_getkeys(add);
	KNO_DO_CHOICES(key,keys) {
	  if (KNO_STRINGP(key)) {
	    lispval val = kno_get(add,key,KNO_VOID);
	    if (KNO_STRINGP(val)) {
	      char *dict_string = (enc==NULL) ? (u8_strdup(CSTRING(key))) :
		u8_localize_string(enc,CSTRING(key),CSTRING(key)+CSTRLEN(key));
	      char *affix_string = (enc==NULL) ? (u8_strdup(CSTRING(val))) :
		u8_localize_string(enc,CSTRING(val),CSTRING(val)+CSTRLEN(val));
	      int rv = Hunspell_add_with_affix(h,dict_string,affix_string);}
	    kno_decref(val);}}
	kno_decref(keys);}
      else NO_ELSE;}}

  struct KNO_HUNSPELLER *kh = u8_alloc(struct KNO_HUNSPELLER);
  kh->c_handle = h;
  kh->dict_path = prefix;
  kh->dict_encoding = enc;
  return kno_wrap_pointer(kh,sizeof(struct KNO_HUNSPELLER),
			  recycle_hunspeller,
			  hunspeller_typetag,
			  prefix);
}

static lispval convert_stringlist(u8_encoding enc,
				  int n,char **strings,
				  int sorted)
{
  lispval each[n], result;
  int i = 0; while (i<n) {
    char *string = strings[i];
    if (enc) {
      size_t len=strlen(strings[i]);
      u8_string converted = u8_make_string(enc,strings[i],strings[i]+len);
      each[i]=kno_init_string(NULL,-1,converted);}
    else each[i]=knostring(strings[i]);
    i++;}
  if (sorted)
    return kno_make_vector(n,each);
  else return kno_make_choice(n,each,KNO_CHOICE_ISCONSES|KNO_CHOICE_DOSORT);
}

typedef int (*hunspellfn)(Hunhandle *h,char ***,const char *);
typedef int (*hunspellfn2)(Hunhandle *h,char ***,const char *,const char *);

lispval hunspell_wrapper(hunspellfn hunspell_fn,
			 lispval hs,lispval term,
			 lispval sorted,
			 u8_context caller)
{
  kno_hunspeller kh = (kno_hunspeller) KNO_RAWPTR_VALUE(hs);
  const char *dict_string = (kh->dict_encoding) ?
    (u8_localize_string
     (kh->dict_encoding,CSTRING(term),CSTRING(term)+CSTRLEN(term))) :
    ((unsigned char *)(CSTRING(term)));
  char **stringlist = NULL;
  int n = hunspell_fn(kh->c_handle,&stringlist,dict_string);
  if (kh->dict_encoding) u8_free(dict_string);
  if (n<0) {
    return kno_err(HunspellError,caller,kh->dict_path,term);}
  lispval result = convert_stringlist
    (kh->dict_encoding,n,stringlist,(KNO_FALSEP(sorted)));
  Hunspell_free_list(kh->c_handle,&stringlist,n);
  return result;
}

lispval hunspell_wrapper2(hunspellfn2 hunspell_fn,
			  lispval hs,lispval term,lispval term2,
			  lispval sorted,
			  u8_context caller)
{
  kno_hunspeller kh = (kno_hunspeller) KNO_RAWPTR_VALUE(hs);
  const char *dict_string = (kh->dict_encoding) ?
    (u8_localize_string
     (kh->dict_encoding,CSTRING(term),CSTRING(term)+CSTRLEN(term))) :
    ((unsigned char *)(CSTRING(term)));
  const char *dict_string2 = (kh->dict_encoding) ?
    (u8_localize_string
     (kh->dict_encoding,CSTRING(term2),CSTRING(term2)+CSTRLEN(term2))) :
    ((unsigned char *)(CSTRING(term2)));
  char **stringlist = NULL;
  int n = hunspell_fn(kh->c_handle,&stringlist,dict_string,dict_string2);
  if (kh->dict_encoding) u8_free(dict_string);
  if (n<0) {
    return kno_err(HunspellError,caller,kh->dict_path,term);}
  lispval result = convert_stringlist
    (kh->dict_encoding,n,stringlist,(KNO_FALSEP(sorted)));
  Hunspell_free_list(kh->c_handle,&stringlist,n);
  return result;
}

DEFC_PRIM("hunspell-stem",hunspell_stem_prim,
	  KNO_MAX_ARGS(3)|KNO_MIN_ARGS(2),
	  "Returns the stem of *term* provided by *hunspell*. "
	  "Returns a vector if *sorted* is provided and not false, otherwise "
	  "returns a choice of string values.",
	  {"hunspell",kno_hunspeller_type,KNO_VOID},
	  {"term",kno_string_type,KNO_FALSE},
	  {"sorted",kno_any_type,KNO_FALSE})
static lispval hunspell_stem_prim(lispval hs,lispval term,lispval sorted)
{
  return hunspell_wrapper(Hunspell_stem,hs,term,sorted,
			  "hunspell_stem_prim");
}

DEFC_PRIM("hunspell-analyze",hunspell_analyze_prim,
	  KNO_MAX_ARGS(3)|KNO_MIN_ARGS(2),
	  "Returns a morphological analysis of *term* provided by *hunspell*. "
	  "Returns a vector if *sorted* is provided and not false, otherwise "
	  "returns a choice of string values.",
	  {"hunspell",kno_hunspeller_type,KNO_VOID},
	  {"term",kno_string_type,KNO_FALSE},
	  {"sorted",kno_any_type,KNO_FALSE})
static lispval hunspell_analyze_prim(lispval hs,lispval term,lispval sorted)
{
  return hunspell_wrapper(Hunspell_analyze,hs,term,sorted,
			  "hunspell_analyze_prim");
}

DEFC_PRIM("hunspell-suggest",hunspell_suggest_prim,
	  KNO_MAX_ARGS(3)|KNO_MIN_ARGS(2),
	  "Suggests alternate correct spellings for *term*. "
	  "Returns a vector if *sorted* is provided and not false, otherwise "
	  "returns a choice of string values.",
	  {"hunspell",kno_hunspeller_type,KNO_VOID},
	  {"term",kno_string_type,KNO_FALSE},
	  {"sorted",kno_any_type,KNO_FALSE})
static lispval hunspell_suggest_prim(lispval hs,lispval term,lispval sorted)
{
  return hunspell_wrapper(Hunspell_suggest,hs,term,sorted,
			  "hunspell_suggest_prim");
}

DEFC_PRIM("hunspell-generate",hunspell_generate_prim,
	  KNO_MAX_ARGS(4)|KNO_MIN_ARGS(2),
	  "Generates morphological variants based on rules. "
	  "Returns a vector if *sorted* is provided and not false, otherwise "
	  "returns a choice of string values.",
	  {"hunspell",kno_hunspeller_type,KNO_VOID},
	  {"term",kno_string_type,KNO_FALSE},
	  {"term2",kno_string_type,KNO_FALSE},
	  {"sorted",kno_any_type,KNO_FALSE})
static lispval hunspell_generate_prim(lispval hs,lispval term,lispval term2,lispval sorted)
{
  return hunspell_wrapper2(Hunspell_generate,hs,term,term2,sorted,
			   "hunspell_generate_prim");
}

/* suggest(suggestions, word) - search suggestions
 * input: pointer to an array of strings pointer and the (bad) word
 *   array of strings pointer (here *slst) may not be initialized
 * output: number of suggestions in string array, and suggestions in
 *   a newly allocated array of strings (*slts will be NULL when number
 *   of suggestion equals 0.)
 */
/*
LIBHUNSPELL_DLL_EXPORTED int Hunspell_suggest(Hunhandle* pHunspell,
                                              char*** slst,
					      const char* word); */

/* morphological functions */

/* analyze(result, word) - morphological analysis of the word */

/* LIBHUNSPELL_DLL_EXPORTED int Hunspell_stem2(Hunhandle* pHunspell,
                                            char*** slst,
                                            char** desc,
					    int n); */

/* generate(result, word, word2) - morphological generation by example(s) */

static lispval hunspeller_module;

long long hunspeller_initialized = 0;

KNO_EXPORT int kno_init_hunspeller()
{
  if (hunspeller_initialized) return 0;
  else hunspeller_initialized = u8_millitime();

  hunspeller_typetag = kno_intern("%HUNSPELLER");

  kno_hunspeller_typeinfo=kno_register_tag_type
    (hunspeller_typetag,kno_hunspeller_type);

  kno_register_config("HUNSPELL:DATA","Where the hunspell module gets its data",
		      kno_sconfig_get,kno_sconfig_set,
		      &default_hunspell_dir);

  hunspeller_module = kno_new_cmodule("hunspeller",0,kno_init_hunspeller);

  link_local_cprims();

  kno_finish_module(hunspeller_module);

  u8_register_source_file(_FILEINFO);

  return 1;
}

static void link_local_cprims()
{
  KNO_LINK_CPRIM("hunspell/open",hunspell_open_prim,3,hunspeller_module);
  KNO_LINK_CPRIM("hunspell-stem",hunspell_stem_prim,3,hunspeller_module);
  KNO_LINK_CPRIM("hunspell-analyze",hunspell_stem_prim,3,hunspeller_module);
  KNO_LINK_CPRIM("hunspell-suggest",hunspell_suggest_prim,3,hunspeller_module);
  KNO_LINK_CPRIM("hunspell-generate",hunspell_generate_prim,4,hunspeller_module);
}
